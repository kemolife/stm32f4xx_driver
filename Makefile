# -----------------------------------------------------------------------------
# Command-line build, same compiler flags as the STM32CubeIDE Debug config.
#
#   make                    build the default app (APP below)
#   make APP=gpio_led       build one app from Src/
#   make apps               build every app in Src/
#   make tests              build the on-target test image
#   make flash APP=...      build and flash one app (ST-LINK + openocd)
#   make test               build, flash and run the tests, print the result
#   make size APP=...       show flash / RAM use
#   make clean
#
# Output goes to build/: build/<app>.elf, .bin, .map
# Needs: arm-none-eabi-gcc, make, openocd (flash / test only)
# -----------------------------------------------------------------------------

APP     ?= uart_tx
BUILD   := build

CC      := arm-none-eabi-gcc
OBJCOPY := arm-none-eabi-objcopy
SIZE    := arm-none-eabi-size
OPENOCD := openocd
BOARD   := board/st_nucleo_f4.cfg

MCU     := -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard
DEFS    := -DDEBUG -DSTM32 -DSTM32F4 -DSTM32F446RETx -DNUCLEO_F446RE
INCS    := -Idrivers/Inc -IInc -Itests/target

CFLAGS  := $(MCU) $(DEFS) $(INCS) -std=gnu11 -g3 -O0 -Wall \
           -ffunction-sections -fdata-sections -MMD -MP
ASFLAGS := $(MCU) -g3 -x assembler-with-cpp
LDFLAGS := $(MCU) -TSTM32F446RETX_FLASH.ld --specs=nosys.specs --specs=nano.specs \
           -Wl,--gc-sections -static -Wl,--start-group -lc -lm -Wl,--end-group

# Linked into every image: drivers, newlib stubs, vector table
COMMON_SRC := $(wildcard drivers/Src/*.c) Src/syscalls.c Src/sysmem.c
COMMON_OBJ := $(COMMON_SRC:%.c=$(BUILD)/obj/%.o) $(BUILD)/obj/Startup/startup_stm32f446retx.o

# Every file in Src/ with a main() is an app
APPS       := $(filter-out syscalls sysmem,$(basename $(notdir $(wildcard Src/*.c))))

TEST_OBJ   := $(patsubst %.c,$(BUILD)/obj/%.o,$(wildcard tests/target/*.c))

.PHONY: all apps tests flash test size clean
.SECONDARY:

all: $(BUILD)/$(APP).elf $(BUILD)/$(APP).bin
	@$(SIZE) $(BUILD)/$(APP).elf

apps: $(APPS:%=$(BUILD)/%.elf)
	@$(SIZE) $^

tests: $(BUILD)/tests.elf
	@$(SIZE) $<

flash: $(BUILD)/$(APP).elf
	$(OPENOCD) -f $(BOARD) -c "program $< verify reset exit"

test:
	tests/run_target.sh

size: $(BUILD)/$(APP).elf
	@$(SIZE) $<

clean:
	rm -rf $(BUILD)

# ---- images -----------------------------------------------------------------

$(BUILD)/tests.elf: $(TEST_OBJ) $(COMMON_OBJ)
	$(CC) $^ $(LDFLAGS) -Wl,-Map=$(@:.elf=.map) -o $@

$(BUILD)/%.elf: $(BUILD)/obj/Src/%.o $(COMMON_OBJ)
	$(CC) $^ $(LDFLAGS) -Wl,-Map=$(@:.elf=.map) -o $@

$(BUILD)/%.bin: $(BUILD)/%.elf
	$(OBJCOPY) -O binary $< $@

# ---- objects ----------------------------------------------------------------

$(BUILD)/obj/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/obj/%.o: %.s
	@mkdir -p $(dir $@)
	$(CC) $(ASFLAGS) -c $< -o $@

-include $(shell find $(BUILD) -name '*.d' 2>/dev/null)
