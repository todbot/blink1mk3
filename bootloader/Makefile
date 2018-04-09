####
#
#
#

.PHONY: all build prep prep-submodule clean distclean

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

clean:
	make -C tomu-bootloader/toboot -f Makefile.toboot-blink1.mk clean

distclean: clean
	echo "distclean"
