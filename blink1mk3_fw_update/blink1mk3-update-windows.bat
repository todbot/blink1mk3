;
;
@echo off

echo ------
echo Putting blink(1) into bootloader mode
.\tools\blink1-tool.exe --gobootload

echo ------
echo Waiting for bootloader USB reconnect...
timeout 5 > NUL

echo ------
echo Programming new firmware
.\tools\dfu-util.exe -v --device 27B8:01ED --download *dfu

echo ------
echo Done!
timeout 5 > NUL
