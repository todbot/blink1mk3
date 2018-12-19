# Overview
This code produces a program that provides a generic bi-directional USB HID data platform,
such as that used in the blink(1) USB LED.

The HID functionality is: a single 8-byte FEATURE report sent and received by the device.

Based on the original `tomu-samples/efm32hg-blinky-usb` example.

# How to Compile
Edit the `Makefile` in the parent directory and possible define where you've installed the toolchain, then :

```
make
```

# How to Flash
Follow the steps in the general README.md, using `blink1mk3.bin`
or type `make program-jlink` to use a Jlink to program the EFM32HG dev board.
