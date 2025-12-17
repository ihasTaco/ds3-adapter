# Supported Hardware

## Whats recommended for this project?

The PS3 and most controllers still use Bluetooth BR/EDR (BT Classic). We use USB OTG (On The Go) to put the board into peripheral mode so the console thinks it's a controller.

When trying to find a board to use with this project, it is recommended to find one that supports both bluetooth classic and usb otg. Without bluetooth classic, in most cases you wont be able to connect the controller to the board, and without OTG support you will not be able to connect the board to the PS3.

Currently bare metal boards aren't supported, but I have plans to port the project over soon. If you don't see a board that supports bt classic and usb otg, add it below!

Tested column key: <br>
```✅ - Tested and works```
```➖ - Untested but should work```
```✖️ - Not yet supported (Needs porting)```

| Device               | Bluetooth BR/EDR? | USB OTG? | Cheap (under $20)? | Tested? | OS/Build Type | Notes |
| ---------------------|:-----------------:|:--------:|:------------------:|:-------:|:-------------:|:---------|
| Raspberry Pi Zero 2W  | ✅ | ✅ | ✅ | ✅ | Linux | Project is built using the Zero 2W. Requires the Zero 2W to be powered by another source, the PS3 usb ports doesn't output enough power. |
| Raspberry Pi Zero W   | ✅ | ✅ | ✅ | ➖ | Linux | Untested but should work. The chip on the Zero W is single core, may cause latency concerns. |
| Raspberry Pi Pico W   | ✅ | ✅ | ✅ | ✖️ | RTOS  | Bare Metal - will require porting |
| Raspberry Pi Pico 2 W | ✅ | ✅ | ✅ | ✖️ | RTOS  | Bare Metal - will require porting |
| Raspberry Pi 4/5      | ✅ | ✅ | ✖️ | ➖ | Linux | Untested but should work. This is very overkill for this kind of project |
| ESP32-DevKitC         | ✅ | ✖️ | ✅ | ✖️ | RTOS  | Bare Metal - will require porting. Would need a MAX3421E USB Controller |
| ESP32-S3              | ✖️ | ✅ | ✅ | ✖️ | RTOS  | Bare Metal - will require porting. Not recommended, requires an external BT Classic module, and no good options for HID. :( |
