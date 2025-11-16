
CC = gcc
CFLAGS = -Wall -Wextra -std=gnu17 -O2
TFLAGS = -Wall -Wextra -std=gnu17 -g -DTEST
SRCS = arg_parser.c main.c data_loader.c hashtable.c dns_forwarder.c dns_parser.c string_normalization.c
OBJS = $(SRCS:.c=.o)
TARGET = dns
TEST_TARGET = dns_test
TEST_SCRIPT = tests/test.py

all: $(TARGET)
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean rule 
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
