# LRbase62

A base-62 comptability mod for LR2.
> [!WARNING]
> This is an experimental mod; use this at your own risk. Please report any bugs you might encounter!

## Usage

LRbase62 is provided as a DLL to inject into LR2. [lr2_chainloader](https://github.com/SayakaIsBaka/lr2_chainload) is recommended as it allows to automatically load LRbase62 during startup without using any extra binaries.
To use LRbase62 with lr2_chainload, follow the steps below:
- Download the latest release of lr2_chainload [here](https://github.com/SayakaIsBaka/lr2_chainload/releases)
- Extract the contents of the archive in LR2's folder
- Download the latest release of LRbase62 [here](https://github.com/SayakaIsBaka/LRbase62/releases)
- Put the DLL file somewhere on your system (it doesn't have to be in LR2's folder)
- Edit `chainload.txt` in LR2's folder and add a line with the path to LRbase62's DLL
- Run the game. Base-62 charts should now play properly.

You may also use LRbase62 by injecting the DLL into LR2's process with your favorite DLL injector.

## Limitations

- Base-62 charts will not work during courses (they will be parsed as regular BMS charts)
- SP-to-DP and Extra Mode may not work as expected (especially with base-62 charts using over 1295 keysounds)

It is very unlikely that these limitations will ever be solved. For non-base-62 charts, LR2 should behave as usual; patches are only applied when the selected chart is a base-62 one and when it's not a course.