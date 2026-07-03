CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c99

# Target executable name
TARGET = sched

# Default compilation target
all: $(TARGET)

$(TARGET): src/sched.c src/sched.h
	$(CC) $(CFLAGS) src/sched.c -o $(TARGET)

# Clean up generated files
clean:
	rm -f $(TARGET)