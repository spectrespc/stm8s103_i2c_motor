# Host unit tests (gcc). Independent of the firmware build in the repo root.

CC      = gcc
CFLAGS  = -Wall -Wextra -std=c99 -O2 -I../src/core -I../src/i2c -I../src/motor

TARGETS = test_i2c_protocol test_stepper_logic

test: $(TARGETS)
	./test_i2c_protocol
	./test_stepper_logic

test_i2c_protocol: test_i2c_protocol.c ../src/core/main.h ../src/i2c/i2c_proto.h ../src/motor/stepper_logic.h
	$(CC) $(CFLAGS) -o $@ test_i2c_protocol.c

test_stepper_logic: test_stepper_logic.c ../src/core/main.h ../src/motor/stepper_logic.h
	$(CC) $(CFLAGS) -o $@ test_stepper_logic.c

clean:
	rm -f $(TARGETS)

.PHONY: test clean