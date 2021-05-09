# Copyright 2016 Google Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

TARGET=blink1mk3

# Use make V=1 for a verbose build.
ifndef V
        Q_CC		= @echo '      CC ' $@;
        Q_OBJDUMP	= @echo ' OBJDUMP ' $@;
        Q_OBJCOPY	= @echo ' OBJCOPY ' $@;
endif

# Define CROSS_COMPILE, CC, OBJCOPY and OBJDUMP
include ../paths.mk

################################################################################
#
# ARGUMENTS TO COMPILERS/LINKERS
#
IFLAGS = -I. \
				 -I../Gecko_SDK/platform/CMSIS/Include \
				 -I../Gecko_SDK/platform/Device/SiliconLabs/EFM32HG/Include \
				 -I../Gecko_SDK/platform/emlib/inc \
				 -I../Gecko_SDK/platform/middleware/usb_gecko/inc \
				 -I../Gecko_SDK/hardware/kit/common/drivers

# Define CFLAGS, LSCRIPT and LFLAGS
include ../compile_flags.mk
# override
LSCRIPT = ./blink1mk3.ld

################################################################################
#
# PHONY TARGETS
#
.PHONY: all $(TARGET) check clean size nm program-jlink program-openocd program-dfu

all: $(TARGET) size

$(TARGET): $(TARGET).bin $(TARGET).dump $(TARGET).elf 

check:
	@echo "Checking Gecko SDK linker script $(LSCRIPT) to ensure FLASH ORIGIN is set to 0x00004000..."
	@grep -E '^\s*FLASH.*ORIGIN\s+=\s+0x' $(LSCRIPT)
	@grep -q -E '^\s*FLASH.*ORIGIN\s+=\s+0x0*4000' $(LSCRIPT) || exit 1
	@echo "Checking Gecko SDK linker script $(LSCRIPT) to ensure FLASH LENGTH is set to 0xC000..."
	@grep -E '^\s*FLASH.*LENGTH\s+=\s+0x' $(LSCRIPT)
	@grep -q -E '^\s*FLASH.*LENGTH\s+=\s+0x0*C000' $(LSCRIPT) || exit 1
	@echo "Checking Gecko SDK linker script $(LSCRIPT) to ensure RAM ORIGIN is set to 0x20000000..."
	@grep -E '^\s*RAM.*ORIGIN\s+=\s+0x' $(LSCRIPT)
	@grep -q -E '^\s*RAM.*ORIGIN\s+=\s+0x20000000' $(LSCRIPT) || exit 1
	@echo "Checking Gecko SDK linker script $(LSCRIPT) to ensure RAM LENGTH is set to 0x2000..."
	@grep -E '^\s*RAM.*LENGTH\s+=\s+0x' $(LSCRIPT)
	@grep -q -E '^\s*RAM.*LENGTH\s+=\s+0x0*2000' $(LSCRIPT) || exit 1
	@echo "Checking ARM Toolchain is installed and accessible..."
	@echo " -> GCC"
	@test -x $(CC) || exit 1
	@echo " -> OBJCOPY"
	@test -x $(OBJCOPY) || exit 1
	@echo " -> OBJDUMP"
	@test -x $(OBJDUMP) || exit 1
	@echo "All checks pass."

clean:
	@echo "Cleaning .dfu files..."
	@rm -f *.dfu
	@echo "Cleaning .bin files..."
	@rm -f *.bin
	@echo "Cleaning .dump files..."
	@rm -f *.dump
	@echo "Cleaning .d files..."
	@rm -f *.d
	@echo "Cleaning .elf files..."
	@rm -f *.elf
	@echo "Cleaning .map files..."
	@rm -f *.map
	@echo "Cleaning .o files..."
	@rm -f *.o

clean-gecko:
	@echo "Cleaning Gecko .o files..."
	@rm -f $(GECKO_OBJ)

cleangecko:
	@echo "Cleaning Gecko_SDK files..."
	@find ../Gecko_SDK -type f -name \*.bin -delete
	@find ../Gecko_SDK -type f -name \*.dump -delete
	@find ../Gecko_SDK -type f -name \*.d -delete
	@find ../Gecko_SDK -type f -name \*.elf -delete
	@find ../Gecko_SDK -type f -name \*.map -delete
	@find ../Gecko_SDK -type f -name \*.o -delete



################################################################################
#
# REAL TARGETS // COMMON
#
GECKO_A_SRC = ../Gecko_SDK/platform/Device/SiliconLabs/EFM32HG/Source/GCC/startup_efm32hg.S
GECKO_C_SRC = $(shell [ -d ../Gecko_SDK ] && find ../Gecko_SDK/platform/emlib/src ../Gecko_SDK/platform/middleware/usb_gecko/src -name \*.c \! -name em_int.c ) \
							../Gecko_SDK/platform/Device/SiliconLabs/EFM32HG/Source/system_efm32hg.c \
							../Gecko_SDK/hardware/kit/common/drivers/capsense.c 
GECKO_OBJ = $(patsubst %.S,%.o,$(GECKO_A_SRC)) \
						$(patsubst %.c,%.o,$(GECKO_C_SRC))

BLINK1_A_SRC = $(shell find . -maxdepth 1 -name \*.S)
BLINK1_C_SRC = $(shell find . -maxdepth 1 -name \*.c)
BLINK1_OBJ = $(patsubst %.S,%.o,$(BLINK1_A_SRC)) $(patsubst %.c,%.o,$(BLINK1_C_SRC))

%.o: %.c
	$(Q_CC)$(CC) -c $(CFLAGS) -c -o $@ $<

%.o: %.S
	$(Q_CC)$(CC) -c $(CFLAGS) -c -o $@ $<


################################################################################
#
# REAL TARGETS // CLEAN
#
$(TARGET).dump: $(TARGET).elf 
	$(Q_OBJDUMP)$(OBJDUMP) -Sx $^ > $@

$(TARGET).elf: $(BLINK1_OBJ) $(GECKO_OBJ)
	$(Q_CC)$(CC) -Xlinker -Map=$(TARGET).map $(LFLAGS) -o $@ $^

$(TARGET).bin: $(TARGET).elf 
	$(Q_OBJCOPY)$(OBJCOPY) -O binary $< $@


####################################################
#
# Programming targets
#

# fixme: does this actually work? do I need to do --pad-to on toboot-blink1.bin first?
joinbin: fullbin.bin
	cat ../../bootloader/toboot-blink1.bin $(TARGET).bin > fullbin.bin

jlink: program-jlink

program-jlink:
#	echo "loadbin $(TARGET).bin,0x4000 \ngo\nexit\n" > jlink-tmp.txt
	echo " power on \n halt \n loadbin $(TARGET).bin,0x4000 \n r \n power off \n exit \n" > jlink-tmp.txt
	jlinkexe -nogui 1 -device EFM32HG309F64 -if swd -speed 4000 -autoconnect 1 -commanderscript jlink-tmp.txt
	rm jlink-tmp.txt

program-openocd:
	openocd -f ../openocd.cfg -c "program $(TARGET).bin verify reset exit 0x4000"

# uses ThingM blink(1) DFU VID:PID
# the '-' before a command means "continue even if it fails"
dfu: program-dfu
program-dfu: bin-to-dfu
	-blink1-tool --gobootload && sleep 1.3
	echo "waiting for bootloader to come online"
	-sleep 3
	dfu-util -v --device 27B8:01EE --download $(TARGET).dfu

bin-to-dfu:
	@echo "converting $(TARGET).bin to $(TARGET).dfu"
	cp $(TARGET).bin $(TARGET).dfu
	dfu-suffix -v 27B8 -p 01EE -a $(TARGET).dfu

# save the entire memory contents to a file
savefull:
	echo " power on \n savebin $(TARGET)-full.bin,0x0,0xffff \n exit \n" > jlink-tmp.txt
	jlinkexe -device EFM32HG309F64 -if swd -speed 4000 -autoconnect 1 -commanderscript jlink-tmp.txt
	rm jlink-tmp.txt

# program the entire memory contents from a file
progfull:
	echo " power on \n erase \n loadbin $(TARGET)-full.bin,0x0 \n go \n power off \n exit \n" > jlink-tmp.txt
	jlinkexe -nogui 1 -device EFM32HG309F64 -if swd -speed 4000 -autoconnect 1 -commanderscript jlink-tmp.txt
	rm jlink-tmp.txt

erase:
	echo " power on \n erase \n go \n power off \n exit \n" > jlink-tmp.txt
	jlinkexe -device EFM32HG309F64 -if swd -speed 4000 -autoconnect 1 -commanderscript jlink-tmp.txt
	rm jlink-tmp.txt


size:
	arm-none-eabi-size  $(TARGET).elf

nm:
	arm-none-eabi-nm -S --numeric-sort -s $(TARGET).elf

#nm:
#	arm-none-eabi-nm -n $(TARGET).elf 

