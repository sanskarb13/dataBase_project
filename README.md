# kvstore — Simple Persistent Key-Value Store

A minimal key-value database written in C, built as part of the *Build Your Own Database* project.

## Features

- **Persistent storage** via an append-only log (`data.db`)
- **In-memory index** — a hand-rolled dynamic array (no standard library maps)
- **Last-write-wins** semantics for duplicate keys
- **Log replay** on startup to restore state across restarts

## Build

```bash
make
```

Requires GCC (or any C11-compatible compiler).

## Run

```bash
./kvstore
```

## Commands

| Command              | Description                              |
|----------------------|------------------------------------------|
| `SET <key> <value>`  | Store a value; prints `OK`               |
| `GET <key>`          | Retrieve a value; prints `NULL` if absent |
| `EXIT`               | Quit the program                         |

## Example session

```
SET name Alice
OK
SET age 30
OK
GET name
Alice
GET missing
NULL
EXIT
```

Restart the program and the data is still there:

```
GET name
Alice
```

## Design notes

- **Storage format**: each `SET` appends a line `SET <key> <value>\n` to `data.db`.
- **Index**: a `malloc`/`realloc`-managed array of `{key, value}` string pairs, scanned linearly. Capacity doubles on overflow.
- **No stdlib maps**: `Index` is implemented from scratch in ~60 lines of C.

## File layout

```
kvstore.c   — all source code
Makefile    — build rules
data.db     — append-only log (created at runtime)
```
