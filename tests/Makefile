# Host unit tests (gcc). Independent of the firmware build in the repo root.

CC      = gcc
CFLAGS  = -Wall -Wextra -std=c99 -O2
TARGET  = test_i2c_protocol

test: $(TARGET)
	./$(TARGET)

$(TARGET): test_i2c_protocol.c
	$(CC) $(CFLAGS) -o $(TARGET) test_i2c_protocol.c

clean:
	rm -f $(TARGET)

.PHONY: test clean