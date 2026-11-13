<p align="center">
  <img src="docs/images/logo.svg" alt="ScrapC" width="520">
</p>

<h1 align="center">ScrapC</h1>

<p align="center">A recreation of Scrapland Remastered in C that runs natively on Linux and Windows.</p>

## What is this?

ScrapC is a fan project that rebuilds Scrapland Remastered 1.6.1 (the GOG build) in C. It was made by studying how the original game works, function by function, and writing new code that does the same. It contains none of the original code, and it behaves like the original game.

It is not affiliated with MercurySteam or the publishers.

## What you need

ScrapC does not include any game files. You need the game files of a legally owned Scrapland Remastered, the `.packed` files of the GOG release. Install the game as usual and ScrapC will use its data.

## What it gives you today

- Runs natively on Linux and on Windows, 64 bit.
- A Vulkan renderer. Direct3D 8 is gone.
- Videos play through FFmpeg.
- Its own sound mixer replaces the old FMOD.
- No GOG or Steam account features. The store code was removed.
- A Display option in the video settings: Windowed, Borderless or Fullscreen.
- The window can be resized, and the game renders at the real window size.

## Progress

<p align="center"><img src="docs/images/progress.svg" alt="ScrapC progress" width="840"></p>

More than four fifths of the whole project is done (the graphic shows 89%). The recreation in C, the clean code, the Vulkan renderer with FFmpeg videos and the own mixer, and the Linux and 64 bit builds are done or nearly done (open: Ghidra symbols and some checks on the PC). The port of the Python scripts to C (issue #2) is at 99% and Python 1.5.2 is gone from the game, the build and the repo. Every game and level script is C or data, and 310 pack modules are made in C. The interpreter went first (the tag interp-final keeps the last state with it), then the object library: module variables, functions, classes, instances, modules, errors, the function tables of the engine and the ports, long integers, the number rules of 1.5.2 and the library modules (math, strop, builtins, sys, imp, signal) are native C, and errno, new, regex, _locale and marshal were dropped because nothing used them. Mods can change data (.def and .mod files, assets) but cannot ship Python scripts. Only tools/ghidra/python152 keeps the 1.5.2 headers for the Ghidra import of the original exe. A deterministic test mode makes every test run repeat byte for byte, so each change is compared exactly against the build before it. The free-roam multiplayer (issue #1) is at 38% and is under way on branches that are not merged yet: a new encrypted UDP protocol, headless level instances (`ScrapC -instance:<Level>`), world replication to the clients, and players with client prediction and lag-compensated hits. The old PvP modes work as before. The overall number once went down from 77% because the planned work grew (removing the object library was added), not because something was undone. The percentages are estimates, and each step is weighted by its share of the work (recreation in C 30, clean C code 10, renderer and media 10, Linux and 64 bit 10, Python scripts 25, multiplayer 15).

For developers: the graphic is drawn by `tools/progress/progress.py` from `tools/progress/progress.json`. Change the JSON when a milestone moves and run the script.

## Getting it

There are no ready-made downloads yet. For now you build it yourself. The steps are in [docs/building.md](docs/building.md).

## How to play

Once built, put ScrapC (Linux) or ScrapC.exe (Windows) together with its libraries into the Bin folder of your installed game. Start it from there, the same way you would start the original Scrap.exe.

On Linux, settings and saves go to your user data folder (`XDG_DATA_HOME`, normally `~/.local/share/MercurySteam/Scrapland`).

## Plans

- Free-roam multiplayer, where players meet on the open map instead of only the old PvP modes (issue #1).
- Porting the game's Python script system to C, step by step (issue #2).

## Prior work and thanks

This project builds on the work of the ReScrap community:

- ReScrap ScrapHacks: https://gitdab.com/ReScrap/ScrapHacks (`.packed` format, Kaitai specs, notes for the 2004 exe)
- ReScrap ScrapModLoader: https://gitdab.com/ReScrap/ScrapModLoader
- ReScrap ScrapMods: https://gitdab.com/ReScrap/ScrapMods

## Where the code lives

The code is hosted on Gitea at git.lixsl.net and mirrored to GitHub at https://github.com/Blackwolf1337/scrapland.

## For developers

- [Building and running](docs/building.md)
- [Testing](docs/testing.md)
- [Development notes](docs/development.md)
- [All documentation](docs/README.md)
