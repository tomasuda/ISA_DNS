# Compiler
CC = gcc

# Normal compilation flags
CFLAGS = -Wall -Wextra -std=gnu17 -O2

# Test compilation flags
TFLAGS = -Wall -Wextra -std=gnu17 -g -DTEST

# Source file names
SRCS = arg_parser.c main.c data_loader.c hashtable.c dns_forwarder.c dns_parser.c string_normalization.c

# Object file names
OBJS = $(SRCS:.c=.o)

# Final executable name
TARGET = dns
TEST_TARGET = dns_test

# Test script
TEST_SCRIPT = tests/test.py

# Default target
all: $(TARGET)

# Normal build
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

# Compile objects
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean rule (všetko)
clean:
	rm -f *.o *.gcda *.gcno *.gcov $(TARGET) $(TEST_TARGET)
	rm -rf tests/__pycache__ .pytest_cache


# Test build: always rebuild with test flags
test: $(TEST_TARGET)
	@cd tests && python3 test.py


$(TEST_TARGET): $(SRCS)
	$(CC) $(TFLAGS) -o $(TEST_TARGET) $(SRCS)

# Rebuild
rebuild: clean all

.PHONY: all clean test rebuild
