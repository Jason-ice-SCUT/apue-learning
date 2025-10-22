CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -O2
TARGET = file_sync
SOURCES = main.c sync_util.c
HEADERS = sync_util.h

$(TARGET): $(SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES)

.PHONY: clean test

clean:
	rm -f $(TARGET)
	rm -rf test_dir

test: $(TARGET)
	chmod +x test_sync.sh
	./test_sync.sh

all: $(TARGET)