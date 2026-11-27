# Android port source notes

This Android port uses the game source and the five mascot sprite sheets
from the approved PS5 r32 build. Its launcher icon is the approved r32 icon.
The original upstream asset inventory is described in ASSET-PROVENANCE.md;
the five later mascot sheets are outside that original inventory.

Build inputs and exact upstream SDL library versions are listed in
android/build-ci.sh. The Android build uses the SDL2, SDL2_image, SDL2_mixer,
SDL2_ttf, FreeType, and libxmp libraries; their licenses accompany the app.
Source: https://github.com/GUMMIBRI/ClanBomber-PS5/tree/android-port-20261127
