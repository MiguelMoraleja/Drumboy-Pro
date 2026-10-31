# Command-line build for the Drumboy Pro application, mirroring the
# STM32CubeIDE "Debug" configuration in .cproject. CubeIDE keeps working
# as before; this exists for CI and for building without the IDE.
#
#   make                         -> build/app.elf, build/app.bin
#   make package VERSION=1.03    -> build/Drumboy_Pro_Fw_01_03.bin (SD-card update package)
#
# Requires arm-none-eabi-gcc (e.g. Ubuntu: apt install gcc-arm-none-eabi libnewlib-arm-none-eabi libstdc++-arm-none-eabi-newlib).

PREFIX ?= arm-none-eabi-
CC      = $(PREFIX)gcc
CXX     = $(PREFIX)g++
OBJCOPY = $(PREFIX)objcopy
SIZE    = $(PREFIX)size

BUILD  ?= build
TARGET  = $(BUILD)/app

ARCH = -mcpu=cortex-m7 -mthumb -mfpu=fpv5-d16 -mfloat-abi=hard
DEFS = -DDEBUG -DUSE_PWR_LDO_SUPPLY -DUSE_HAL_DRIVER -DSTM32H723xx
INCS = \
	-ICore/Inc \
	-IFATFS/Target \
	-IFATFS/App \
	-IDrivers/STM32H7xx_HAL_Driver/Inc \
	-IDrivers/STM32H7xx_HAL_Driver/Inc/Legacy \
	-IMiddlewares/Third_Party/FatFs/src \
	-IDrivers/CMSIS/Device/ST/STM32H7xx/Include \
	-IDrivers/CMSIS/Include \
	-ICore/Library/Global \
	-ICore/Library/Lcd \
	-ICore/Library/Dac \
	-ICore/Library/Controller

COMMON   = $(ARCH) $(DEFS) $(INCS) -Ofast -ffast-math -ffunction-sections -fdata-sections -Wall --specs=nano.specs -MMD -MP
CFLAGS   = -std=gnu11 $(COMMON)
CXXFLAGS = -std=gnu++14 $(COMMON) -fno-exceptions -fno-rtti -fno-use-cxa-atexit
LDFLAGS  = $(ARCH) -TSTM32H723ZGTX_FLASH.ld --specs=nosys.specs --specs=nano.specs \
           -Wl,--gc-sections -Wl,-Map=$(TARGET).map
LDLIBS   = -Wl,--start-group -lc -lm -lstdc++ -lsupc++ -Wl,--end-group

SRC_DIRS = Core FATFS Middlewares Drivers
C_SRCS   = $(shell find $(SRC_DIRS) -name '*.c' -not -path '*/Templates/*')
CXX_SRCS = $(shell find $(SRC_DIRS) -name '*.cpp')
ASM_SRCS = $(shell find $(SRC_DIRS) -name '*.s')
OBJS     = $(addprefix $(BUILD)/,$(C_SRCS:.c=.o) $(CXX_SRCS:.cpp=.o) $(ASM_SRCS:.s=.o))

VERSION ?=
VERSION_MAJOR = $(word 1,$(subst ., ,$(VERSION)))
VERSION_MINOR = $(word 2,$(subst ., ,$(VERSION)))
PACKAGE = $(BUILD)/Drumboy_Pro_Fw_$(shell printf '%02d_%02d' $(VERSION_MAJOR) $(VERSION_MINOR) 2>/dev/null).bin

.PHONY: all package clean

all: $(TARGET).bin

$(TARGET).elf: $(OBJS) STM32H723ZGTX_FLASH.ld
	$(CXX) $(LDFLAGS) $(OBJS) $(LDLIBS) -o $@
	$(SIZE) $@

$(TARGET).bin: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@

package: $(TARGET).bin
	@test -n "$(VERSION)" || (echo "usage: make package VERSION=<major>.<minor>" && exit 1)
	python3 ../tools/package_firmware.py $< $(PACKAGE) --version $(VERSION)

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/%.o: %.s
	@mkdir -p $(dir $@)
	$(CC) $(ARCH) -x assembler-with-cpp -c $< -o $@

clean:
	rm -rf $(BUILD)

-include $(OBJS:.o=.d)
