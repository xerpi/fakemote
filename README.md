# *_fakemote_*
_An IOS module that fakes Wiimotes from the input of USB game controllers._

## Features

### Supported USB game controllers
| Device Name              | Vendor Name | Vendor ID | Product ID |
|:------------------------:|:-----------:|:---------:|:----------:|
| PlayStation 3 Controller | Sony Corp.  | 054c      | 0268       |
| DualShock 4 [CUH-ZCT1x]  | Sony Corp.  | 054c      | 05c4       |
| DualShock 4 [CUH-ZCT2x]  | Sony Corp.  | 054c      | 09cc       |

- DS3 and DS4 support includes LEDs, rumble, and the accelerometer
- DS4's touchpad is used to emulate the Wiimote IR Camera pointer (only tested with the sensor bar configured on top of the screen)
- Both controllers emulate a Wiimote with the Nunchuk extension connected

## Installation
1) Download [d2x cIOS Installer](https://wii.guide/assets/files/d2x-cIOS-Installer-Wii.zip) and extract it to the SD card
2) Copy `FAKEMOTE.app` to the d2x cIOS Installer directory that contains the modules of the cIOS version you want to install.  
   For example, for `d2x-v10-beta52` copy `FAKEMOTE.app` to `sd:/apps/d2x-cIOS-Installer-Wii/v10/beta52/d2x-v10-beta52`
3) Open d2x cIOS Installer's `ciosmaps.xml` (located at `sd:/apps/d2x-cIOS-Installer-Wii/ciosmaps.xml`) and do the following:
   1) Locate the line containing the base IOS version you want to install. It starts with `<base ios=`.  
      For base IOS 57:
      ```xml
      <base ios="57" version="5918" contentscount="26" modulescount="7">
      ```
   3) Increase `modulescount` by 1.  
      For base IOS 57:
      ```xml
      <base ios="57" version="5918" contentscount="26" modulescount="8">
      ```
   3) Add a `<content>` entry for `FAKEMOTE`after the last `<content module>`.  
      For base IOS 57:
      ```xml
      <content id="0x24" module="FAKEMOTE" tmdmoduleid="-1"/>
      ```
4) Run d2x cIOS Installer and install the cIOS

## Compilation
1) Download and install [devkitARM](https://devkitpro.org/wiki/Getting_Started)
2) Install `stripios`:
   1) Download `stripios`'s source code from [Leseratte's d2xl cIOS](https://github.com/Leseratte10/d2xl-cios/tree/master/stripios)
   2) Compile it:
   ```bash
   g++ main.cpp -o stripios
   ```
   4) Install it:
   ```bash
   cp stripios $DEVKITPRO/tools/bin
   ```
3) Run `make` to compile `FAKEMOTE.app`

## Notes
- This has only been tested with `d2x-v10-beta52` and base IOS 57
- This is still in beta-stage, therefore it might not work as expected

## Credits
- [Dolphin emulator](https://dolphin-emu.org/) developers
- [Wiibrew](https://wiibrew.org/) contributors
- [d2x cIOS](https://github.com/davebaol/d2x-cios) developers
- [Aurelio Mannara](https://twitter.com/AurelioMannara/)
- _neimod_, for their [Custom IOS Module Toolkit](http://wiibrew.org/wiki/Custom_IOS_Module_Toolkit)
- Everybody else who helped me!
- ...and everybody who made Wii's scene a reality! 👍

## Disclaimer
````
THIS APPLICATION COMES WITH NO WARRANTY AT ALL, NEITHER EXPRESSED NOR IMPLIED.
NO ONE BUT YOURSELF IS RESPONSIBLE FOR ANY DAMAGE TO YOUR WII CONSOLE BECAUSE OF A IMPROPER USAGE OF THIS SOFTWARE.
````
