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

################################################################################
#
# Gecko
#
GECKO_SDK_REV = v5.0.0

TARGET = blink1mk3

################################################################################
#
# PHONY TARGETS
#
.PHONY: all $(TARGET) check clean deps

all: deps
	make -C $(TARGET)

clean:
	make -C $(TARGET) clean
	@echo "Cleaning Gecko_SDK files..."
	@find Gecko_SDK -type f -name \*.bin -delete
	@find Gecko_SDK -type f -name \*.dump -delete
	@find Gecko_SDK -type f -name \*.d -delete
	@find Gecko_SDK -type f -name \*.elf -delete
	@find Gecko_SDK -type f -name \*.map -delete
	@find Gecko_SDK -type f -name \*.o -delete


dist-clean:
	make clean || true
	rm -rf Gecko_SDK
#	rm -f $(LSCRIPT)
#	rm -f $(LSCRIPT2)

deps:
	@if [ ! -d Gecko_SDK ]; then \
		git clone --depth 1 https://github.com/SiliconLabs/Gecko_SDK --branch ${GECKO_SDK_REV}; \
	fi
#	@if [ ! -f "${LSCRIPT}" ]; then \
#		echo "Patching linker file to ${LSCRIPT}"; \
#		patch -o ${LSCRIPT} Gecko_SDK/platform/Device/SiliconLabs/EFM32HG/Source/GCC/efm32hg.ld linker.patch; \
#	fi
#	@if [ ! -f "${LSCRIPT2}" ]; then \
#		echo "Patching linker_nobootload file to ${LSCRIPT2}"; \
#		patch -o ${LSCRIPT2} Gecko_SDK/platform/Device/SiliconLabs/EFM32HG/Source/GCC/efm32hg.ld linker_nobootloader.patch; \
#	fi

