#!/bin/sh
cd -- "$(dirname "$0")"

echo "------"
echo "Putting blink(1) into bootloader mode"
./tools/blink1-tool --gobootload

echo "------"
echo "Waiting for bootloader USB reconnect..."
sleep 5

echo "------"
echo "Programming new firmware"
./tools/dfu-util -v --device 27B8:01ED --download *dfu

echo "------"
echo "Done!"
sleep 2
