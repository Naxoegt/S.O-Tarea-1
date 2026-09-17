CC = gcc
CFLAGS = -Wall -Wextra -std=gnu11 -D_GNU_SOURCE
TARGET = mishell

SRCS = main.c parser.c executor.c signals.c jobs.c
OBJS = $(SRCS:.c=.o)
HEADERS = defs.h parser.h executor.h signals.h jobs.h

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
