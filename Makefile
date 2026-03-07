CC      = gcc
CFLAGS  = -Wall -Wextra -Wpedantic -std=c11 -O2

TARGET  = database_project
SRC     = database_project.c

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET) data.db
