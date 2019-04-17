
# blink(1) mk3 -- USB RGB LED notification light

This repo lives at https://github.com/todbot/blink1mk3

blink(1) mk3 is a blink(1) USB device: https://blink1.thingm.com

<img src="./docs/imgs/blink1mk2-twocolor-long.jpg" width="600"><img src="./docs/imgs/blink1-logos.jpg" width="175">

"[blink(1) mk3](https://blink1.thingm.com/) is an update to blink(1).
blink(1) is a super status light:
it packs three dimensions of information
(color, brightness and pattern) into a single tiny package that fits into
any USB port on nearly every device. It makes it incredibly easy to connect
any data source in the cloud or on your computer to a full-color RGB LED so
you can know what's happening without checking any windows, going to any
websites or typing any commands." - Get a blink(1) at https://buy.thingm.com/

More technically, blink(1) is a Smart LED controller with built-in USB firmware. blink(1) has a huge range of support from the Linux kernel to Node.js.

This repo contains:

- firmware -- blink(1) firmware for blink1mk3 (on DEFM32HG309F64, same as Tomu)
- bootloader -- DFU firmware for blink(1), based on tomu-bootloader
- schematic -- schematic diagrams of blink(1) mk3


The official repositories for blink(1) software are:

| repository | description |
| ---------- | ----------- |
| [Blink1Control2](https://github.com/todbot/Blink1Control2) | Graphical app for Mac / Windows / Linux |
| [blink1-tool](https://github.com/todbot/blink1-tool) | Command-line tools & C-library for all platforms |
| [blink1-java](https://github.com/todbot/blink1-java) | Java and Processing library |
| [blink1-python](https://github.com/todbot/blink1-python) | Python library |
| [node-blink1](https://github.com/sandeepmistry/node-blink1) | Node.js library |
| [blink1](https://github.com/todbot/blink1) | Hardware/firmware design for mk1/mk2, misc docs & notes
| [blink1mk3](https://github.com/todbot/blink1mk3) | Hardware/firmware design for mk3





