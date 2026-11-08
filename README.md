# ClanBomber for PS5

Native PS5 homebrew port of the GPL ClanBomber 2 SDL2 lineage. The console title is **ClanBomber**. This release contains local matches against the CPU, selectable CPU difficulty, and PNG character skin overrides.

## Install

Extract `ClanBomber-PS5-v0.1.0.zip` and copy the complete `PPSA77028` folder into the `homebrew` folder at the root of USB0. Launch ClanBomber manually through your PS5 homebrew launcher. The package contains the game data and uses the original ClanBomber logo art.

In **New Game → Local Player Setup**, set one player to the first joystick and another to CPU. Select **Start Local**. Use the D-pad or left stick to move, Cross or Circle to plant a bomb, Options to pause or resume, and Cross to advance the round results. Use the D-pad and Cross in menus. **New Game → CPU Difficulty** changes CPU bomb cadence. See `characters/README.md` for custom skins.

## Source and credits

Game source is based on [the SDL2 fork](https://github.com/viti95/ClanBomber2) as vendored in [the Switch port](https://github.com/Mayo0x0/Clanbomber2-Nintendo-Switch-Port). See [asset provenance](ASSET-PROVENANCE.md), [upstream credits](upstream/README), and [the PS5 build instructions](ps5-native/BUILD.md).

The released executable is the owner-tested r12 build `01.000.012-2a6c4bef`, compiled from game source commit `cd9e9e9e4da1dc70d18a21bc7af015e75fa5d63b`. The published source snapshot adds build instructions and the native link inputs. The newer six-run logging changes are development-only.
