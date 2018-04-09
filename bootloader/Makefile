####
#
#
#

.PHONY: all compile prep prep-submodule check clean distclean


prep-submodule:
	@if [ ! -d tomu-bootloader ]; then \
		git submodule init ; \
  fi

prep-toboot:
	cp toboot-blink1.c tomu-bootloader/toboot
	cp Makefile.toboot-blink1.mk tomu-bootloader/toboot

prep: prep-submodule prep-toboot

compile:
	make -C tomu-bootloader/toboot -f Makefile.toboot-blink1.mk

clean:
	make -C tomu-bootloader/toboot -f Makefile.toboot-blink1.mk clean

distclean: clean
	echo "distclean"
