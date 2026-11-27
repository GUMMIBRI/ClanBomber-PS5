# Custom character skins

Copy your PNG sheet to this folder as `slot-1.png` through `slot-13.png`.
Restart the game and cycle the player's skin in Local Player Setup. The
number matches the position in the existing skin cycle, starting with Snake.
Each file replaces that slot for both human and CPU players.

Use a transparent PNG with 10 columns and 4 rows of frames. Keep the
animation order and alignment of the corresponding original sprite sheet in
`ClanBomber2/pics`. Frames are 40 pixels wide.

- Slots 1 (Snake), 2 (Tux), 4 (BSD), and 9–13 (Chameleon, GNU, Wilber, Fedora, Mandrake) use 40 × 60 pixel frames, 400 × 240 overall.
- Slots 3 (Spider) and 5–8 (Red, Blue, Yellow, Green) use 40 × 40 pixel frames, 400 × 160 overall.

`example-slot-1.png` is a copy of the original Snake sheet. Rename a copy
to `slot-1.png` and edit it to try an import. Missing, unreadable, and
wrong-sized sheets fall back to the built-in art. The example carries the
same provenance and notices as the bundled Snake sheet.
