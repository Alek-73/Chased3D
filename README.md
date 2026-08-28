# Chased3D

A first-person raycast 3D maze for the Atari 8-bit (800XL / 130XE class), written in C and 6502 assembly with [cc65](https://cc65.github.io/).

The player walks freely through a 20 x 59 grid maze rendered in real time, with a top-down minimap and an FPS readout.

## Controls

| Key | Action |
| --- | --- |
| `W` | Walk forward |
| `S` | Walk backward |
| `A` | Turn left |
| `D` | Turn right |
| `Space` | Deploy decoy |
| `Esc` | Quit |

Keys are read straight from POKEY (`SKSTAT` / `KBCODE`) rather than through CIO, so holding a key gives continuous movement without waiting for the OS auto-repeat delay.

Movement is scaled by elapsed jiffies, so walking and turning speed stay constant regardless of the render rate.

## Building

Requires cc65 and PowerShell. The build script looks for `ca65.exe` and `cl65.exe` in `C:\tools\cc65\bin`, or wherever the `CC65_BIN` environment variable points.

```powershell
.\build_chased3d.ps1
```

This produces `Chased3D.XEX`, plus `Chased3D.map` and `Chased3D.lbl` for debugging.

To run it in [Altirra](https://www.virtualdub.org/altirra.html):

```powershell
Altirra64.exe Chased3D.XEX
```

`gen_trig3d.py` regenerates `trig3d.c` / `trig3d.h` (sine, reciprocal, ray-offset and fisheye tables). Only needed if you change the column count or field of view. It requires Python and writes the tables as literal data, so the build itself has no Python dependency.

## Display layout

The screen is a single ANTIC mode D playfield (160 x 96, 4 colours, 2 bits per pixel, 40 bytes per row) with one ANTIC mode 6 text line appended for the HUD:

```
        bytes 0-4          bytes 5-39
      +-----------+------------------------------+
      |  minimap  |        3D view               |  92 mode D rows
      |  20 x 59  |        140 x 92              |
      +-----------+------------------------------+
      |            FPS nn                        |  1 mode 6 row
      +------------------------------------------+
```

The minimap and the 3D view own disjoint bytes of the frame buffer. That matters: the raycaster rewrites its columns every frame, so anything sharing those bytes would be erased and redrawn constantly and visibly flicker.

The total is 24 blank + 92 x 2 + 8 = 216 scanlines, which is the standard Atari display height. Overshooting it would push the HUD into overscan.

Colours are shared between both areas, since a 4-colour mode has no more to give:

| Value | Register | 3D view | Minimap |
| --- | --- | --- | --- |
| 0 | `COLBK` | Ceiling | Corridor |
| 1 | `COLPF0` | Floor | Facing direction |
| 2 | `COLPF1` | Wall, N/S face | Wall |
| 3 | `COLPF2` | Wall, E/W face | Player |

Walls hit on their X face are drawn brighter than those hit on their Y face. That shading is the main depth cue in a flat-shaded raycaster.

## How the renderer works

18 rays are cast per frame, one per 8-pixel-wide column. Everything uses 8.8 fixed point, and angles use a 256-step circle so an 8-bit index wraps naturally.

Each ray runs a DDA grid traversal ([`RAYCAST.ASM`](RAYCAST.ASM)) that steps cell to cell until it hits a wall tile, leaves the map, or exhausts its step budget. The resulting ray length is multiplied by the cosine of its angle relative to the view centre, converting it to a perpendicular distance and removing the fisheye bow.

Some details that matter on a 1.79 MHz 6502:

**Column filling is fully unrolled.** [`COLUMN3D.ASM`](COLUMN3D.ASM) holds two blocks of 92 `sta abs,x` instructions, ascending and descending. Because each store is exactly 3 bytes, the entry tables are just `base + 3 * row`, so jumping into the middle of a block draws a partial column at 5 cycles per row. Ceiling draws upward from the top of the wall and floor downward from its bottom, so every pixel is written exactly once.

**Wall spans are bounded by self-modifying code.** `col3d_fill_span` plants a temporary `RTS` at the row after the span and restores it afterwards. The earlier approach filled to the bottom of the screen and painted the floor back over it, which left wall-coloured fragments visible in the floor whenever ANTIC scanned the buffer mid-update.

**One byte per column, no masking.** At 2 bits per pixel a 4-pixel column is exactly one byte, so filling never needs a read-modify-write.

**The map is indexed without multiplying.** `maze.c` builds split `maze_row_lo[]` / `maze_row_hi[]` address tables. A 2D array index would compile to a multiply by 20 on every DDA step.

**Wall height is a table lookup.** `height_table[dist >> 4]` replaces a 16-bit division per ray.

**Hot values are file-scope statics.** cc65 keeps function locals on a software stack reached through `(sp),y`, which is expensive in an inner loop.

The frame buffer is 4K-aligned and the display list 1K-aligned via segments in [`chased3d.cfg`](chased3d.cfg), because ANTIC cannot fetch display data across a 4K boundary and a display list cannot cross a 1K boundary.

### Performance

About 10 fps on a PAL machine. The progression during development, measured with the on-screen counter:

| Change | fps |
| --- | --- |
| Initial version, DDA in C | 3 |
| Row tables, 8-bit coordinates, inlined tile test | 4.5 |
| DDA moved to assembly | 6 |
| 18 rays, 8x8 multiplies, height lookup table | 10 |

The remaining cost is the per-ray setup still in C. Moving it into `RAYCAST.ASM` alongside the DDA would allow going back to 35 narrower columns at the same frame rate.

There is no double buffering, so some tearing is visible while moving.

## Maze format

Levels are 20 columns by 59 rows. Level 1 is embedded in [`maze.c`](maze.c); levels 2-5 load from `D:L2.CSV` through `D:L5.CSV`, semicolon-separated with a header row and a row index in the first column.

| Tile | Meaning |
| --- | --- |
| 0 | Corridor |
| 1, 2 | Wall (blocks movement and stops rays) |
| 3, 4 | Target |
| 6 | Gate |

Only tiles 1 and 2 are solid. Targets and gates are walkable and currently have no gameplay behaviour.

## Source layout

| File | Purpose |
| --- | --- |
| `chased3d.c` | Entry point, input, movement, collision, FPS timing |
| `view3d.c` / `.h` | Raycaster, display list setup, minimap, HUD |
| `maze.c` / `.h` | Level data, CSV loader, row address tables |
| `trig3d.c` / `.h` | Generated fixed-point tables |
| `RAYCAST.ASM` | DDA grid traversal |
| `COLUMN3D.ASM` | Unrolled column blitter |
| `chased3d.cfg` | Linker config with aligned screen and display list segments |
| `gen_trig3d.py` | Regenerates the trig tables |

## Status

The renderer and movement work. There is no gameplay yet: the pursuer, targets, decoys and scoring from the original 2D *Chased* have not been ported.
