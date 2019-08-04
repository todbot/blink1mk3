;
;
@echo off

echo ------
echo Putting blink(1) into bootloader mode
.\blink1-tool.exe --gobootload

echo ------
echo Waiting for bootloader USB reconnect...
sleep 5

echo ------
echo Programming new firmware
.\dfu-util.exe -v --device 27B8:01ED --download *dfu

echo ------
echo Done!
sleep 2


