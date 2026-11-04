<p align="center">
  <img src="docs/images/logo.svg" alt="ScrapC" width="520">
</p>

<h1 align="center">ScrapC</h1>

<p align="center">A recreation of Scrapland Remastered in C that runs natively on Linux and Windows.</p>

## What is this?

ScrapC is a fan project that rebuilds Scrapland Remastered 1.6.1 (the GOG build) in C. It is not the original source code. It was written from scratch to behave like the original game.

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
