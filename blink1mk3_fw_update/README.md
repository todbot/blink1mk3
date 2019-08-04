
# blink(1) mk3 firmware updates


This is a collection of update bundles to update a blink(1) in the field.

To run an update:

1. Plug in the blink(1) and make sure no other blink(1)-communicating programs are running (e.g. Blink1Control2)

2. Go to the "Releases" section of this repo and download
the update bundle you want. It will be called something like `blink1mk3-update-fw303-2Aug2019`.
The bundle works on MacOS or Windows.

3. Unzip the bundle  ("Extract All" on Windows)

4. Start the update:
  - Windows: Double-click `blink1mk3-update-windows.bat`
  - MacOS: Double-click `blink1mk3-update-macos.command`

5. A Termainal/Command window will open.  You will see it tell the blink(1) to go into bootloader mode, then wait, then upload the new firmware.  
During this process you will see the blink(1) flash alternating purple-pink to indicate bootloader mode, then it will go dark, then it will begin to flash red-green-blue until your OS re-recognizes the blink(1).

6. Done! The firmware update is now complete. You can verify the firmware version by doing `blink1-tool --fwversion`.
