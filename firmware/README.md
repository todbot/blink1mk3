# blink1mk3 firmware

## What is this:

This is firmware for the blink(1) mk3 USB RGB LED device.
It borrows heavily from the [Tomu](http://tomu.im) project.
This directory started as a fork of the
[tomu-samples](https://github.com/im-tomu/tomu-samples) repository.

## How to use:

1. To compile these, you'll need a cross-compiling toolchain to arm-none-eabi.

  - Debian/Ubuntu/... : `sudo apt-get install gcc-arm-none-eabi`
  - Fedora : `sudo yum install arm-none-eabi-gcc-cs arm-none-eabi-newlib`
  - Arch : `sudo pacman -S arm-none-eabi-gcc arm-none-eabi-newlib`
  - Other Linux : check your package manager, or
  - Anything else (Windows, OSX, Linux) : [https://developer.arm.com/open-source/gnu-toolchain/gnu-rm ](https://developer.arm.com/open-source/gnu-toolchain/gnu-rm )

  If the cross compiling toolchain is in your PATH then the Makefile will
  auto-detect it; else (or to override it) specify it using the environment
  variable "CROSS_COMPILE".
  There might be additional dependencies, depending on which specific project
  you're using, and the instructions for compilation are included in each
  project's README.md

2. Run `make deps .` to patch the linker with support for the blink(1) and Tomu
    and clone the [Gecko SDK](https://github.com/SiliconLabs/Gecko_SDK) locally

3. Now you can build the firmware.

4. The production firmware is located in `firmware-v30x`.


## Flashing:

The easiest way to upload a new firmware is to trigger the USB DFU bootloader
and use `dfu-util` to upload the new firmware.  For an example of how to do this,
see the "program-dfu" Makefile target in `firmware-v30x` or the "fw-updates" directory.

You can also solder wires to the programming test pads:
"5V" (+5VDC), "G" (gnd), "D" (SWD), "C" (SWC) pins
then use any JTAG/SWD programmer to upload new code.


## Testing:

In Windows, install the app "TDD.exe"
(https://www.thesycon.de/eng/usb_descriptordumper.shtml)
It is very good at detecting subtle errors in USB configuration that MacOS & Linux
point out.

On Mac OS X, install "USB Prober"
