# *_fakemote_*
_An IOS module that fakes Wiimotes from the input of USB game controllers._

### Supported USB game controllers
| Device Name              | Vendor Name | Vendor ID | Product ID |
|:------------------------:|:-----------:|:---------:|:----------:|
| PlayStation 3 Controller | Sony Corp.  | 054c      | 0268       |
| DualShock 4 [CUH-ZCT1x]  | Sony Corp.  | 054c      | 05c4       |
| DualShock 4 [CUH-ZCT2x]  | Sony Corp.  | 054c      | 09cc       |

## Installation
1) [Download](https://wii.guide/assets/files/d2x-cIOS-Installer-Wii.zip) and copy `d2x-cIOS-Installer-Wii/v10/beta52`  to `sd:/apps/d2x-cIOS-Installer-Wii/v10/beta52/d2x-v10-beta52-fake-wiimote`
2) Apply [this patch](https://pastebin.com/raw/yyEgpyfL) to `sd:/apps/d2x-cIOS-Installer-Wii/ciosmaps.xml`
3) Copy the IOS module (`.app`) to `sd:/apps/d2x-cIOS-Installer-Wii/v10/beta52/d2x-v10-beta52-fake-wiimote/`
4) Run _d2x cIOS installer_ and install `d2x-v10-beta52-fake-wiimote` (tested on base IOS57)

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
3) Run `make` to compile _fakemote_ and generate `FAKEMOTE.app`

## Notes
**This is still in beta-stage, therefore it might not work as expected.**

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
