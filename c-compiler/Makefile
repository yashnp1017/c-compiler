CC      = gcc
CFLAGS  = -Wall -Wextra -Iinclude -g -O2
SRCS    = src/main.c src/lexer.c src/parser.c src/codegen.c
OBJS    = $(SRCS:.c=.o)
TARGET  = cc

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

test: $(TARGET)
	@echo "Running test suite..."
	@python3 tests/run_tests.py

clean:
	rm -f src/*.o $(TARGET) tests/*.s tests/*.out
