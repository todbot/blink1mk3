
# blink(1) mk3 bootloader

This bootloader is equivalent to the Tomu bootloader with a few small changes:
- USB VID/PID different
- Bootloader can ONLY be triggered by either:
  - SWD & SWC pins shorted
  - RAM setting "TOBOOT_FORCE_ENTRY_MAGIC" (via `blink1-tool --gobootload`)

These changes are encapsulated in the files in this directory:
- `usb_desc.h` -- replaces normal `tomu-bootloader/toboot/usb_desc.h`
- `toboot-main-blink1.c` -- replaces normal `tomu-bootloader/toboot/main.c`


