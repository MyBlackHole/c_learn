CC      := gcc
CFLAGS  := -std=gnu11 -O2 -Wall -Wextra -Werror
LDLIBS  :=

.PHONY: all clean

all: relay_reader

relay_reader: relay_reader.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

clean:
	$(RM) relay_reader
