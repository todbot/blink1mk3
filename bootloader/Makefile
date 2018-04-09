####
#
#
#

.PHONY: all build prep prep-submodule clean distclean booster-build

all: build 

prep-submodule:
	@if [ ! -d tomu-bootloader ]; then \
		git submodule update --init ; \
  fi

prep: prep-submodule
	cp toboot-main-blink1.c tomu-bootloader/toboot
	cp Makefile.toboot-blink1.mk tomu-bootloader/toboot

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
