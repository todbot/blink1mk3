####
# 
#
#

.PHONY: all build prep prep-submodule clean distclean booster-build size

all: build booster-build booster

prep-submodule:
	@if [ ! -d tomu-bootloader/toboot ]; then \
		git submodule update --init ; \
  fi

prep: prep-submodule
	cp toboot-main-blink1.c tomu-bootloader/toboot
	cp Makefile.toboot-blink1.mk tomu-bootloader/toboot
	cp usb_desc.h tomu-bootloader/toboot
	cp mcu_too.h tomu-bootloader/toboot
	cp ws2812_spi.h tomu-bootloader/toboot

build: prep
	make -C tomu-bootloader/toboot -f Makefile.toboot-blink1.mk
	cp tomu-bootloader/toboot/toboot-blink1.bin .


clean:
	make -C tomu-bootloader/toboot -f Makefile.toboot-blink1.mk clean

distclean: clean
	echo "distclean"

booster-build:
	cd tomu-bootloader/booster && make && gcc make-booster.c -o make-booster && cd ..

booster:
	cd tomu-bootloader/booster && ./make-booster ../toboot/toboot-blink1.bin ../../toboot-blink1-booster.bin
#	./tomu-bootloader/booster/make-booster ./tomu-bootloader/toboot/toboot-blink1.bin ./toboot-blink1-booster.bin
	cp toboot-blink1-booster.bin toboot-blink1-booster.dfu
	dfu-suffix -v 27B8 -p 01EE -a toboot-blink1-booster.dfu

# Uses ThingM blink(1) DFU bootloader VID/PID
dfu-booster:
	dfu-util -v --device 27B8:01EE --download toboot-blink1-booster.dfu

program-openocd:
	openocd -f ../firmware/openocd.cfg -c 'program toboot-blink1.bin verify reset exit'

program-jlink:
#	echo "loadbin toboot-blink1.bin,0 \n r \nexit\n" > jlink-tmp.txt
	echo "power on \n st \n loadbin toboot-blink1.bin,0 \n st \nr \n power off \n st \n exit\n" > jlink-tmp.txt
	jlinkexe -device EFM32HG309F64 -if swd -speed 4000 -autoconnect 1 -commanderscript jlink-tmp.txt
	rm jlink-tmp.txt

jlink: program-jlink
openocd: program-openocd

size:
	arm-none-eabi-size ./tomu-bootloader/toboot/toboot-blink1.elf

# for when we're powering it for first time, have jlink supply 5V
# echo "power on\n; loadbin toboot-blink1,0 \nr \nexit\n" > jlink-tmp.txt
