SPL_ROOT = $(HOME)/STM8-SPL-SDCC
SPL_INC  = $(SPL_ROOT)/Libraries/STM8S_StdPeriph_Driver/inc
SPL_LIB  = $(SPL_ROOT)/Libraries/STM8S_StdPeriph_Driver/STM8S103/spl.lib
DEVICE   = STM8S103

CC       = sdcc
CFLAGS   = -mstm8 --std-sdcc11 --opt-code-size -D$(DEVICE)

# Source modules, searched in this order (project headers must resolve before
# the SPL includes). Our stm8s_conf.h lives in src/core.
SRC_DIRS = src/core src/hal src/i2c src/motor
INCLUDE  = $(addprefix -I,$(SRC_DIRS)) -I$(SPL_INC)
VPATH    = $(SRC_DIRS)

BUILD_DIR = build

SRC      = main.c hw_config.c i2c_slave.c stepper.c eeprom.c stm8s_it.c
OBJ      = $(addprefix $(BUILD_DIR)/,$(SRC:.c=.rel))
HEADERS  = main.h stepper.h stepper_logic.h eeprom.h i2c_slave.h i2c_proto.h hw_config.h stm8s_it.h stm8s_conf.h
TARGET   = $(BUILD_DIR)/firmware.ihx

all: $(TARGET)

# Real file target: the firmware link depends on $(SPL_LIB), so make orders the
# link after the SPL build (safe under `make -j`, and builds it when missing).
$(SPL_LIB):
	$(MAKE) -C $(SPL_ROOT)/Libraries/STM8S_StdPeriph_Driver DEVICE=$(DEVICE)

spl_lib: $(SPL_LIB)

$(BUILD_DIR)/%.rel: %.c $(HEADERS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDE) -c $< -o $@

$(TARGET): $(OBJ) $(SPL_LIB)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJ) -L$(dir $(SPL_LIB)) -lspl

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

flash: $(TARGET)
	stm8flash -c stlinkv2 -p stm8s103f3 -s flash -w $(TARGET)

verify: $(TARGET)
	stm8flash -c stlinkv2 -p stm8s103f3 -s flash -v $(TARGET)

clean:
	rm -rf $(BUILD_DIR)
	$(MAKE) -C $(SPL_ROOT)/Libraries/STM8S_StdPeriph_Driver clean DEVICE=$(DEVICE)

.PHONY: all clean flash verify spl_lib