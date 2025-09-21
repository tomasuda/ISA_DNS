# Compiler
CC = gcc

# Compilation flags
CFLAGS = -Wall -Wextra -std=gnu17 -O2

# Source file names
SRCS = arg_parser.c main.c data_loader.c hashtable.c dns_forwarder.c dns_parser.c string_normalization.c

# Object file names
OBJS = $(SRCS:.c=.o)

# Final executable name
TARGET = dns

# Main rule
all: $(TARGET)

# Rule for creating the executable
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

# Rule for compiling .c files to .o files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean rule (remove object and executable files)
clean:
	rm -f $(OBJS) $(TARGET)

# Rebuild rule
rebuild: clean all