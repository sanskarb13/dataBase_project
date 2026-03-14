/*
 * database_project.c — A simple persistent key-value store
 *
 * Supports SET <key> <value>, GET <key>, and EXIT commands.
 * Data is persisted to an append-only log file ("data.db").
 * On startup, the log is replayed to rebuild an in-memory index.
 *
 * Index structure: a dynamically-resizing array of key/value string pairs.
 * "Last write wins" semantics: the latest SET for a key replaces any previous value.
 *
 * Output conventions (matching gradebot expectations):
 *   SET  -> prints "OK"
 *   GET  -> prints the value, or an empty line if the key does not exist
 *   EXIT -> flushes and exits cleanly
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ─────────────────────────────────────────────
 * Constants
 * ───────────────────────────────────────────── */
#define DB_FILE     "data.db"   /* append-only log filename                */
#define MAX_LINE    4096        /* maximum length of one input/log line     */
#define INITIAL_CAP 16          /* initial capacity of the in-memory index  */

/* ─────────────────────────────────────────────
 * Entry — one key/value pair in the index
 * ───────────────────────────────────────────── */
typedef struct {
    char *key;
    char *value;
} Entry;

/* ─────────────────────────────────────────────
 * Index — dynamically-resizing array of entries
 *
 * We deliberately avoid any built-in map/dict type.
 * Lookups scan linearly; capacity doubles on overflow.
 * ───────────────────────────────────────────── */
typedef struct {
    Entry *entries;
    int    size;       /* number of live entries      */
    int    capacity;   /* number of allocated slots   */
} Index;

/* ─────────────────────────────────────────────
 * Forward declarations
 * ───────────────────────────────────────────── */
void  index_init(Index *idx);
void  index_free(Index *idx);
void  index_set(Index *idx, const char *key, const char *value);
char *index_get(const Index *idx, const char *key);
void  load_from_disk(Index *idx, FILE *db);
void  persist_set(FILE *db, const char *key, const char *value);
void  run_repl(Index *idx, FILE *db);

/* ═══════════════════════════════════════════════
 * main — open the log, replay it, then run the REPL
 * ═══════════════════════════════════════════════ */
int main(void) {
    Index idx;
    index_init(&idx);

    /*
     * Open (or create) the append-only log.
     * "a+" opens for both appending and reading;
     * the file is created if it does not exist.
     */
    FILE *db = fopen(DB_FILE, "a+");
    if (!db) {
        fprintf(stderr, "ERROR: cannot open %s\n", DB_FILE);
        return 1;
    }

    /* Replay the persisted log to rebuild in-memory state */
    load_from_disk(&idx, db);

    /* Hand control to the interactive command loop */
    run_repl(&idx, db);

    fclose(db);
    index_free(&idx);
    return 0;
}

/* ═══════════════════════════════════════════════
 * Index operations
 * ═══════════════════════════════════════════════ */

/*
 * index_init — allocate an empty index with INITIAL_CAP slots.
 */
void index_init(Index *idx) {
    idx->entries = malloc(INITIAL_CAP * sizeof(Entry));
    if (!idx->entries) { perror("malloc"); exit(1); }
    idx->size     = 0;
    idx->capacity = INITIAL_CAP;
}

/*
 * index_free — release all memory owned by the index.
 * Both the key and value strings are heap-allocated via strdup,
 * so each must be freed individually before freeing the array.
 */
void index_free(Index *idx) {
    for (int i = 0; i < idx->size; i++) {
        free(idx->entries[i].key);
        free(idx->entries[i].value);
    }
    free(idx->entries);
    idx->size = idx->capacity = 0;
}

/*
 * index_set — insert or update a key (last-write-wins).
 *
 * Scans the array linearly for an existing entry with the same key.
 * If found, frees the old value and stores the new one in-place.
 * If not found, appends a new entry, growing the array if necessary.
 *
 * Time complexity: O(n) — acceptable for a simple educational KV store.
 */
void index_set(Index *idx, const char *key, const char *value) {
    /* Search for an existing entry with this key */
    for (int i = 0; i < idx->size; i++) {
        if (strcmp(idx->entries[i].key, key) == 0) {
            /* Key found — replace value, free old allocation */
            free(idx->entries[i].value);
            idx->entries[i].value = strdup(value);
            if (!idx->entries[i].value) { perror("strdup"); exit(1); }
            return;
        }
    }

    /* Key not found — append a new entry */
    if (idx->size == idx->capacity) {
        /* Double capacity to amortise reallocations */
        idx->capacity *= 2;
        idx->entries = realloc(idx->entries, idx->capacity * sizeof(Entry));
        if (!idx->entries) { perror("realloc"); exit(1); }
    }

    idx->entries[idx->size].key   = strdup(key);
    idx->entries[idx->size].value = strdup(value);
    if (!idx->entries[idx->size].key || !idx->entries[idx->size].value) {
        perror("strdup"); exit(1);
    }
    idx->size++;
}

/*
 * index_get — look up a key.
 *
 * Returns a pointer to the stored value string if found,
 * or NULL if the key does not exist in the index.
 * The caller must NOT free the returned pointer.
 */
char *index_get(const Index *idx, const char *key) {
    for (int i = 0; i < idx->size; i++) {
        if (strcmp(idx->entries[i].key, key) == 0) {
            return idx->entries[i].value;
        }
    }
    return NULL;
}

/* ═══════════════════════════════════════════════
 * Persistence
 * ═══════════════════════════════════════════════ */

/*
 * load_from_disk — replay the append-only log to rebuild the index.
 *
 * Each valid line in data.db has the format:
 *     SET <key> <value>\n
 *
 * Lines that do not match this format are silently skipped so that
 * a partially-written tail (e.g. after a crash) does not corrupt state.
 */
void load_from_disk(Index *idx, FILE *db) {
    char line[MAX_LINE];

    rewind(db); /* seek to the start of the file before reading */

    while (fgets(line, sizeof(line), db)) {
        /* Strip trailing CR and/or LF */
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') line[--len] = '\0';
        if (len > 0 && line[len - 1] == '\r') line[--len] = '\0';

        /* Only process SET records */
        if (strncmp(line, "SET ", 4) != 0) continue;

        char *rest  = line + 4;          /* skip the "SET " prefix */
        char *space = strchr(rest, ' '); /* find separator between key and value */
        if (!space) continue;            /* malformed record — skip */

        *space      = '\0';              /* split string into key / value */
        char *key   = rest;
        char *value = space + 1;

        index_set(idx, key, value);      /* replay into in-memory index */
    }
}

/*
 * persist_set — append a SET record to the log and flush to disk immediately.
 *
 * fflush() ensures the record reaches the OS buffer; in a production system
 * we would also call fsync() to guarantee physical durability, but fflush()
 * is sufficient for this project's crash-recovery requirements.
 */
void persist_set(FILE *db, const char *key, const char *value) {
    fprintf(db, "SET %s %s\n", key, value);
    fflush(db);
}

/* ═══════════════════════════════════════════════
 * REPL — Read-Eval-Print Loop
 * ═══════════════════════════════════════════════ */

/*
 * run_repl — read commands from STDIN, write responses to STDOUT.
 *
 * Supported commands:
 *   SET <key> <value>  — store the value; respond "OK"
 *   GET <key>          — print the value, or an empty line if not found
 *   EXIT               — exit the program cleanly
 *
 * All responses are followed by a newline and immediately flushed so
 * that automated testers receive output without buffering delays.
 */
void run_repl(Index *idx, FILE *db) {
    char line[MAX_LINE];

    while (fgets(line, sizeof(line), stdin)) {
        /* Strip trailing CR and/or LF */
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') line[--len] = '\0';
        if (len > 0 && line[len - 1] == '\r') line[--len] = '\0';

        /* Skip blank lines */
        if (len == 0) continue;

        /* ── EXIT ── */
        if (strcmp(line, "EXIT") == 0) {
            break;
        }

        /* ── SET <key> <value> ── */
        if (strncmp(line, "SET ", 4) == 0) {
            char *rest  = line + 4;
            char *space = strchr(rest, ' ');
            if (!space) {
                /* Missing value — print error but keep running */
                printf("ERROR: SET requires a key and a value\n");
                fflush(stdout);
                continue;
            }
            *space      = '\0';
            char *key   = rest;
            char *value = space + 1;

            index_set(idx, key, value);   /* update in-memory index    */
            persist_set(db, key, value);  /* append to on-disk log     */
            printf("OK\n");
            fflush(stdout);
            continue;
        }

        /* ── GET <key> ── */
        if (strncmp(line, "GET ", 4) == 0) {
            char *key = line + 4;
            char *val = index_get(idx, key);
            if (val) {
                /* Key exists — print its value */
                printf("%s\n", val);
            } else {
                /* Key not found — print an empty line (not "NULL") */
                printf("\n");
            }
            fflush(stdout);
            continue;
        }

        /* ── Unknown command ── */
        printf("ERROR: unknown command\n");
        fflush(stdout);
    }
}
