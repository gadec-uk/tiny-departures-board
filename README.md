# tiny-departures-board [![License Badge](https://img.shields.io/badge/BY--NC--SA%204.0%20License-grey?style=flat&logo=creativecommons&logoColor=white)](https://creativecommons.org/licenses/by-nc-sa/4.0/)

This is an tiny (00 gauge model railway size) live Departures Board replicating those at many UK railway stations (using data provided by National Rail's public API) and UK wide bus stops (using data provided by bustimes.org). This implementation uses a 0.91" OLED display panel with SSD1306 display controller onboard, driven by an ESP32-C3 SuperMini processor board. STL files are also provided for 3D printing the custom case. A small number of pre-assembled departure boards are also available exclusively from our [store](https://store.gadec.co.uk).

This project is based on the table top Departures Board available [here](https://github.com/gadec-uk/departures-board)

## Features
* All processing is done onboard by the ESP32-C3 processor
* Smooth animation matching the real departures boards
* Displays up to the next 9 departures with scheduled time, destination, calling stations and expected departure time
* Optionally only show services calling at a selected railway station
* Optionally only show services departing from selected platform numbers 
* Displays Network Rail service messages
* Train information (operator, class, number of coaches etc.)
* In Bus mode, displays up to the next 9 departures with service number, destination and schedule/expected time
* Fully-featured browser based configuration screens - choose any station on the UK rail network / UK Bus Stops
* Automatic firmware updates (optional)
* Displays the weather at the selected location (optional)
* STL files provided for custom 3D printed case

![Image](https://github.com/user-attachments/assets/f4fabf02-3911-45e2-9f1a-7ed02eca8736)

<img src="https://github.com/user-attachments/assets/915c58fe-fd02-41ae-9dbd-a4226ed1ebf7" align="center">

## Quick Start

### What you'll need

1. An ESP32-C3 SuperMini board. For example from [AliExpress](https://www.aliexpress.com/item/1005007663345442.html).
2. A 0.91" 128x32 OLED Display Panel with SSD1306 display controller onboard. For example from [AliExpress](https://www.aliexpress.com/item/1005008640132638.html).
3. A 3D printed case using the [STL](https://github.com/gadec-uk/tiny-departures-board/tree/main/stl) files provided. There are two version of the case - use the deeper version if you have a OLED panel with pre-soldered header pins (and you don't want to remove them). If you don't have a 3D printer, the cases are available from our [store](https://store.gadec.co.uk) or you can use a 3D print service.
4. A National Rail Darwin Lite API token (these are free of charge - request one [here](https://realtime.nationalrail.co.uk/OpenLDBWSRegistration)). The board will only operate in Bus mode without a token.
5. Optionally, an OpenWeather Map API token to display weather conditions at the selected station or bus stop (these are also free, sign-up for a free developer account [here](https://home.openweathermap.org/users/sign_up)).
6. Some intermediate soldering skills.

<img src="https://github.com/user-attachments/assets/ca62021b-3b69-4d16-9f55-130701f6005f" align="center">

### Wiring Guide

Solder the four wires to the **BACK** of the OLED panel (or directly to the header pins if present). You cannot use Dupont connectors, they will not fit the custom case design. Feed the wires through the opening in the case and down through the post. I recommend that you use 30AWG wires as these will thread easily through the duct in the post. For permanent installations (model railways), rather than using the USBC connection, you can power the SuperMini board directly via its 5V and G connections (you will need a good quality 5V source capable of supplying at least 500mA). Do not connect an external power supply at the same time as using the USBC connector.
<img src="https://github.com/user-attachments/assets/0202b9c4-831d-43e2-a745-589766c34e5a" align="center">

### Installing the firmware

The project uses the Arduino framework and the ESP32 v3.2.0 core. If you want to build from source, you'll need [PlatformIO](https://platformio.org).

Alternatively, you can download pre-compiled firmware images from the [releases](https://github.com/gadec-uk/tiny-departures-board/releases). These can be installed over the USB serial connection using [esptool](https://github.com/espressif/esptool). If you have python installed, install with *pip install esptool*. For convenience, a pre-compiled executable version for Windows is included [here](https://github.com/gadec-uk/tiny-departures-board/tree/main/esptool).

Attach the ESP32-C3 SuperMini board via it's USB port and use the following command to flash the firmware:

```
esptool.py --chip esp32c3 --baud 460800 write_flash -z \
  0x0000 bootloader.bin \
  0xe000 boot_app0.bin \
  0x8000 partitions.bin \
  0x10000 firmware.bin
```

The tool should automatically find the correct serial port. If it fails to, you can manually specify the correct port by adding *--port COMx* (replace *COMx* with your actual port, e.g. COM3, /dev/ttyUSB0, etc.).

If using the pre-compiled esptool.exe version on Windows, save the esptool.exe and the four firmware (.bin) files to the same directory. Open a command prompt (Windows Key + R, type cmd and press enter) and change to the directory you saved the files into. Now type the following command on one line and press enter:
```
.\esptool --chip esp32c3 --baud 460800 write_flash -z 0x0000 bootloader.bin 0xe000 boot_app0.bin 0x8000 partitions.bin 0x10000 firmware.bin
```

Subsequent updates can be carried out automatically over-the-air or you can manually update from the Web GUI.

### First time configuration

WiFiManager is used to setup the initial WiFi connection on first boot. The ESP32 will broadcast a temporary WiFi network named "Departures Board", connect to the network and follow the on-screen instuctions. You can also watch a video walkthrough of setup and configuration process below.

[![Tiny Departures Board Setup Video](https://github.com/user-attachments/assets/d72efaa5-143c-4769-99b4-29d22d5fdb06)](https://youtu.be/3kOFSdS7M2M)

Once the ESP32-C3 has established an Internet connection, the next step is to enter your API keys (if you do not enter a National Rail token, the board will only operate in Bus mode). Finally, select a station location. Start typing the location name and valid choices will be displayed as you type.

### Web GUI

At start-up, the ESP32's IP address is displayed. To change the station or to configure other miscellaneous settings, open the web page at that address. The settings available are:
- **Board Mode** - switch between National Rail Departures and UK Bus Stops modes
- **Station** - start typing a few characters of a station name and select from the drop-down station picker displayed (National Rail mode).
- **Only show services calling at** - filter services based on *calling at* location (National Rail mode - if you want to see the next trains *to* a particular station).
- **Only show these platforms** - filter services based on the platform they depart from. Note: there are many services for which platform number is not supplied, these would also be filtered out.
- **Bus Stop ATCO code** - type the ATCO number of the bus stop you want to monitor (see [below](#bus-stop-atco-codes) for details).
- **Only show these Bus services** - filter buses by service numbers (enter a list of the service numbers, comma separated).
- **Recently verfied ATCO codes** - quickly select from recently used bus stop ATCO codes.
- **Brightness** - adjusts the brightness of the OLED screen.
- **Include bus replacement services** - optionally include bus replacement services (National Rail mode).
- **Include current weather at location** - this option requires a valid OpenWeather Map API key (National Rail/Bus mode).
- **Increase API refresh rate** - reduces the interval between data refreshes (National Rail mode). Uses more data and is not usually required.
- **Suppress calling at / information messages** - removes all horizontally scrolling text (much lower functionality but less distracting).
- **Flip the display 180°** - rotates the display (the case design provides two different viewing angles depending on orientation).
- **Set custom hostname for this board** - change the hostname from the default "TinyDeparturesBoard", useful if you are running multiple boards.
- **Custom (non-UK) time zone (only for clock)** - if you're not based in the UK you can set the clock to display in your local time zone (see [below](#custom-time-zones) for details).
- **Enable automatic firmware updates at startup** - automatically checks for AND installs the latest firmware from this repository.

A drop-down menu (top-right) adds the following options:
- **Check for Updates** - manually checks for and installs any updates to the firmware.
- **Edit API Keys** - view/edit your National Rail and OpenWeather Map API keys.
- **Clear WiFi Settings** - deletes the stored WiFi credentials and restarts in WiFiManager mode (useful to change WiFi network).
- **Display Alignment** - displays a test screen to aid alignment of the panel in the case.
- **Restart System** - restarts the ESP32.

#### Other Web GUI Endpoints

A few other urls have been implemented, primarily for debugging/developer use:
- **/factoryreset** - deletes all configuration information, api keys and WiFi credentials. The entire setup process will need to be repeated.
- **/update** - for manual firmware updates. Download the latest binary from the [releases](https://github.com/gadec-uk/departures-board/releases). Only the **firmware.bin** file should be uploaded via */update*. The other .bin files are not used for upgrades. This method is *not* recommended for normal use.
- **/info** - displays some basic information about the current running state.
- **/formatffs** - formats the filing system, erasing the configuration files (but not the WiFi credentials).
- **/dir** - displays a (basic) directory listing of the file system with the ability to view/delete files.
- **/upload** - upload a file to the file system.

### Bus Stop ATCO codes
Every UK bus stop has a unique ATCO code number. To find the ATCO code of the stop you want to monitor, go to [bustimes.org/search](https://bustimes.org/search) and type a location in the search box. Select the location from the list of places shown and then select the particular stop you want from the list. The ATCO code is shown on the stop information page. After entering the code in the Departures Board setup screen, tap the **Verify** button and the location will be shown confirming your selection. You must use the **Verify** button *before* you can save changes. Up to ten of the most recently verified ATCO codes are saved and can be selected from a dropdown list for quick access.

<img src="https://github.com/user-attachments/assets/8a41ec6d-5f15-4102-b3d5-c09260986319" align="center">

### Custom Time Zones
To set a custom time zone for the departure board clock, you will need to enter the POSIX time zone string for your location. Some examples are `CST6CDT,M3.2.0/2,M11.1.0/2` for Canada (Central Time) and `AEST-10AEDT,M10.1.0,M4.1.0/3` for Australia (Eastern Time). The easiest way to find the correct syntax is to ask your favourite AI chat engine *"What is the POSIX time zone string for ..."*. Note that changing the time zone only affects the clock (and date) display. Service times are *always* shown in UK time.

### Donating

<a href="https://buymeacoffee.com/gadec.uk"><img src="https://github.com/user-attachments/assets/e5960046-051a-45af-8730-e23d4725ab53" align="left" width="160" style="margin-right: 15px;" /></a>
This software is completely free for non-commercial use without obligation. If you would like to support me and encourage ongoing updates, you can [buy me a coffee!](https://buymeacoffee.com/gadec.uk)

### Licence
This work is licensed under **Creative Commons Attribution-NonCommercial-ShareAlike 4.0**. To view a copy of this licence, visit [https://creativecommons.org/licenses/by-nc-sa/4.0/](https://creativecommons.org/licenses/by-nc-sa/4.0/). Note: the terms of the licence prohibit commericial use of this work, this includes *any* reselling of the work in kit or assembled form for commercial gain.
