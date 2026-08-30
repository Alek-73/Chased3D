# Chased3D program map

This map shows which parts of Chased3D are game code, cc65 runtime support,
Atari OS services, DOS 2.5 services, and direct hardware access.

## Layer map

```mermaid
flowchart TB
    subgraph GAME["Chased3D game code"]
        MAIN["chased3d.c<br/>main loop, input, movement,<br/>pursuer and game state"]
        MAZE["maze.c<br/>levels, collision, exit,<br/>DOS CSV loading"]
        VIEW["view3d.c<br/>camera, display list,<br/>ray projection, minimap, HUD"]
        SPRITE["sprite3d.c<br/>targets, pursuer, decoy,<br/>laser and PMG projection"]
        MUSIC["melody.c<br/>melody parser and<br/>sound state"]
        TRIG["trig3d.c<br/>generated fixed-point<br/>trig and reciprocal tables"]
    end

    subgraph ASM["6502 assembly fast paths"]
        RAY["RAYCAST.ASM<br/>DDA ray setup and<br/>maze traversal"]
        COLUMN["COLUMN3D.ASM<br/>unrolled vertical<br/>column blitter"]
        DLI["FLOORDLI.ASM<br/>display-list interrupt<br/>floor color bands"]
        VBI["MELODY.ASM<br/>deferred VBI audio tick"]
    end

    subgraph CRT["cc65 Atari C runtime"]
        START["startup and exit<br/>XEX entry, BSS, C stack,<br/>constructors, main"]
        LIBC["stdlib<br/>abs, rand, srand"]
        STDIO["stdio<br/>fopen, fgetc, fclose"]
        ATARI["atari.h helpers<br/>waitvsync, get_tv,<br/>OS and chip structs"]
    end

    subgraph OS["Atari OS ROM and shadow RAM"]
        SETVBV["SETVBV $E45C<br/>install deferred VBI"]
        SHADOW["OS shadows and vectors<br/>SDLST, SDMCTL, GPRIOR,<br/>colors, RTCLOK, VVBLKD"]
        CIO["CIO and D: device<br/>file I/O through IOCBs"]
    end

    subgraph DOS["Atari DOS 2.5"]
        LOADER["binary loader<br/>loads Chased3D.XEX<br/>and calls RUNAD"]
        FMS["file management system<br/>opens and reads D:L3.CSV<br/>through D:L5.CSV"]
        DISK["disk files<br/>L3.CSV, L4.CSV, L5.CSV"]
    end

    subgraph HW["Atari 8-bit hardware"]
        CPU["6502 CPU and RAM"]
        ANTIC["ANTIC<br/>display-list and PMG DMA,<br/>DLI, WSYNC, NMIEN"]
        GTIA["GTIA<br/>playfield colors,<br/>players and missiles"]
        POKEY["POKEY<br/>keyboard, random,<br/>four audio channels"]
        PIA["PIA PORTB<br/>BASIC ROM / RAM control"]
    end

    MAIN --> MAZE
    MAIN --> VIEW
    MAIN --> SPRITE
    MAIN --> MUSIC
    MAIN --> LIBC
    MAIN --> ATARI
    VIEW --> TRIG
    VIEW --> RAY
    VIEW --> COLUMN
    VIEW --> DLI
    SPRITE --> TRIG
    SPRITE --> VIEW
    SPRITE --> LIBC
    MUSIC --> VBI

    START --> MAIN
    MAZE --> STDIO
    STDIO --> CIO
    CIO --> FMS
    FMS --> DISK
    LOADER --> START

    ATARI --> SHADOW
    VBI --> SETVBV
    SETVBV --> SHADOW
    SHADOW --> ANTIC
    SHADOW --> GTIA
    MAIN -->|"SKSTAT, KBCODE, RANDOM"| POKEY
    MAIN -->|"PORTB"| PIA
    VIEW --> ANTIC
    VIEW --> GTIA
    DLI --> ANTIC
    SPRITE --> ANTIC
    SPRITE --> GTIA
    VBI --> POKEY
    CPU --- GAME
    CPU --- ASM

    classDef game fill:#d7ead3,stroke:#315a2d,color:#152813
    classDef asm fill:#ffe0a8,stroke:#8a5812,color:#352104
    classDef crt fill:#d9e8f5,stroke:#35617f,color:#132b3b
    classDef os fill:#e8ddf2,stroke:#674982,color:#291b35
    classDef dos fill:#f2d8d5,stroke:#89453d,color:#351916
    classDef hw fill:#dedede,stroke:#4a4a4a,color:#171717
    class MAIN,MAZE,VIEW,SPRITE,MUSIC,TRIG game
    class RAY,COLUMN,DLI,VBI asm
    class START,LIBC,STDIO,ATARI crt
    class SETVBV,SHADOW,CIO os
    class LOADER,FMS,DISK dos
    class CPU,ANTIC,GTIA,POKEY,PIA hw
```

Arrows into the hardware layer mean direct memory-mapped access. Arrows through
the OS or DOS layers mean a service call or an OS-maintained shadow register.
The game does not use CIO for keyboard input; it reads POKEY directly so held
keys are visible without the OS key-repeat delay.

## Main-loop map

```mermaid
flowchart LR
    BOOT["DOS loads XEX"] --> CRT["cc65 startup"] --> INIT["main initialization"]
    INIT --> LEVEL["load level and build<br/>map, targets, laser, exit"]
    LEVEL --> VIDEO["install display list,<br/>PMG, DLI and audio VBI"]
    VIDEO --> TICK

    subgraph FRAME["one game-loop pass"]
        TICK["read RTCLOK<br/>calculate frame_ticks"] --> KEY["read POKEY keyboard"]
        KEY --> SIM["movement, decoy, laser,<br/>pursuer and threat"]
        SIM --> RULES["catch, collect targets,<br/>open and reach exit"]
        RULES --> RENDER["raycast walls"]
        RENDER --> OBJECTS["draw PMG objects"]
        OBJECTS --> MAP["update minimap and FPS"]
        MAP --> VSYNC["waitvsync"]
    end

    VSYNC --> TICK
    KEY -->|"Esc"| RESTORE["restore PORTB"] --> EXIT["return to cc65 / DOS"]
```

The VBI audio routine runs asynchronously even when rendering takes several
vertical blanks. The DLI runs at selected floor scanlines, independently of
both the game loop and the VBI.

```mermaid
sequenceDiagram
    participant Loop as C main loop
    participant OS as Atari OS VBI
    participant Audio as MELODY.ASM
    participant ANTIC as ANTIC display DMA
    participant Floor as FLOORDLI.ASM
    participant POKEY as POKEY audio

    Loop->>Loop: update and render frame
    ANTIC-->>Floor: DLI at floor-band boundary
    Floor->>ANTIC: WSYNC then write COLPF0
    OS-->>Audio: deferred VBI vector 7
    Audio->>POKEY: update AUDF1-4 and AUDC1-4
    Audio-->>OS: chain to saved deferred VBI
    Loop->>OS: waitvsync()
```

## Source responsibilities

| Part | Responsibility | Important boundary |
| --- | --- | --- |
| [`chased3d.c`](chased3d.c) | Program entry, frame timing, keyboard, player and pursuer simulation, level transitions | Reads POKEY and PIA directly; uses OS jiffy clock |
| [`maze.c`](maze.c) | 20 x 59 tile map, embedded levels 1-2, CSV levels 3-5, collision and exit state | Uses cc65 stdio, which reaches DOS through CIO |
| [`view3d.c`](view3d.c) | 18-ray renderer, display list, frame buffer, minimap, HUD and floor motion | Calls the DDA, blitter and DLI assembly routines; configures ANTIC and color shadows |
| [`sprite3d.c`](sprite3d.c) | Object placement and projection, wall occlusion, Player/Missile graphics | Writes ANTIC and GTIA registers plus PMG RAM |
| [`melody.c`](melody.c) | Converts melody strings to frequency and duration arrays | Shares state with the interrupt-driven assembly player |
| [`trig3d.c`](trig3d.c) | 8.8 sine, reciprocal, ray-angle and fisheye-correction tables | Generated by [`gen_trig3d.py`](gen_trig3d.py) |
| [`RAYCAST.ASM`](RAYCAST.ASM) | Fixed-point ray setup and DDA wall traversal | Reads C maze row-pointer tables directly |
| [`COLUMN3D.ASM`](COLUMN3D.ASM) | Self-modifying, unrolled screen-column fill | Writes the C-owned frame buffer directly |
| [`FLOORDLI.ASM`](FLOORDLI.ASM) | Six rotating floor luminance bands | Owns the DLI vector and writes `WSYNC` / `COLPF0` |
| [`MELODY.ASM`](MELODY.ASM) | Melody, threat and laser audio ticks | Installs through OS `SETVBV`, then writes POKEY directly |
| [`chased3d.cfg`](chased3d.cfg) | XEX format, segment placement, alignment, stack and DOS reservation | Keeps video RAM under the disabled BASIC ROM |
| [`build_chased3d.ps1`](build_chased3d.ps1) | Assembles four assembly files and links all C modules | Produces the XEX, label file and linker map |

## Runtime, OS, and DOS boundaries

### cc65 C runtime

The `-t atari` runtime supplies the XEX startup and exit path, the software C
stack, `waitvsync()`, `get_tv()`, `rand()` / `srand()`, and stdio. Function
locals normally live on cc65's software stack, which is why the renderer keeps
hot per-ray values in static storage. The linker configuration reserves a 1 KB
stack immediately below `$A000`.

The runtime is also the adapter between ISO C file calls and Atari I/O:

```mermaid
flowchart LR
    LOAD["maze_load_level()"] --> FOPEN["fopen / fgetc / fclose"]
    FOPEN --> CRTIO["cc65 Atari stdio and fd layer"]
    CRTIO --> CIO["Atari OS CIO / IOCB"]
    CIO --> DDEV["DOS 2.5 D: handler / FMS"]
    DDEV --> CSV["L3.CSV, L4.CSV, L5.CSV"]
```

### Atari OS

The program deliberately mixes OS cooperation with direct hardware access:

| OS location/service | Use |
| --- | --- |
| `RTCLOK` low byte `$0014` | Elapsed jiffies, speed scaling and FPS timing |
| `VDSLST` `$0200` | DLI vector used by the floor color routine |
| `VVBLKD` `$0224` | Existing deferred VBI saved and chained by the audio routine |
| `SDMCTL`, `SDLST`, `GPRIOR`, color shadows | OS-side copies of ANTIC/GTIA display state |
| `NOCLIK` `$02DB` | Disables the OS keyboard click |
| `SETVBV` `$E45C` | Atomically installs deferred VBI vector 7 |
| CIO | Used indirectly by cc65 stdio for `D:` level files |

### DOS 2.5

DOS has two roles. First, its binary loader reads `Chased3D.XEX`, processes the
load segments, and jumps through the XEX run address to cc65 startup. Second,
while the game runs, DOS's resident file-management system services the `D:`
device requests used for levels 3-5. Levels 1 and 2 need no disk access because
they are generated from embedded data.

The program itself does not call DOS menu or DUP commands. DOS remains below
the game in memory and is reached only through the normal OS CIO path.

## Hardware register map

| Chip | Registers used | Purpose |
| --- | --- | --- |
| POKEY | `AUDF1-AUDF4` / `AUDC1-AUDC4` `$D200-$D207`, `AUDCTL` `$D208` | Four-channel melody, threat tone and laser noise |
| POKEY | `KBCODE` `$D209`, `RANDOM` `$D20A`, `SKSTAT/SKCTL` `$D20F` | Continuous keyboard polling, random seed and keyboard/audio initialization |
| GTIA | `HPOSP0-3` `$D000-$D003`, missile positions `$D004-$D007`, sizes `$D008-$D00C` | Positions and scales billboards and the decoy |
| GTIA | `GRACTL` `$D01D`, `PRIOR` `$D01B`, `COLPF0` `$D016`, player/playfield colors | Enables PMG and selects priorities and colors |
| ANTIC | `DMACTL` `$D400`, `PMBASE` `$D407`, `WSYNC` `$D40A`, `NMIEN` `$D40E` | Display/PMG DMA, aligned PMG base, DLI timing and interrupt enable |
| PIA | `PORTB` `$D301` | Exposes RAM at `$A000-$BFFF` by disabling BASIC ROM |

## Memory map

The addresses below come from [`chased3d.cfg`](chased3d.cfg). Exact used ends
move as code changes; [`Chased3D.lbl`](Chased3D.lbl) and the generated
`Chased3D.map` are the authoritative build outputs.

```text
$0000 +------------------------------+
      | OS zero page                 |
$0082 +------------------------------+
      | cc65 ZEROPAGE / EXTZP        |  $0082-$00FF
$0100 +------------------------------+
      | CPU hardware stack           |
$0200 +------------------------------+
      | OS vectors and shadow RAM    |
$0600 +------------------------------+
      | PAGE6 DLI code/data/BSS       |  $0600-$06FF
$0700 +------------------------------+
      | DOS 2.5 and OS-managed RAM   |
$1C20 +------------------------------+
      | DOS-reserved boundary        |
$4000 +------------------------------+
      | XEX startup, game code,      |
      | tables, data and ordinary BSS|
$9C00 +------------------------------+
      | 1 KB cc65 software C stack   |  grows downward toward $9C00
$A000 +------------------------------+
      | SCREEN, DLIST, PMGRAM,        |  RAM exposed beneath BASIC ROM
      | HIGHBSS                      |
$C000 +------------------------------+
      | OS ROM / hardware windows    |
$FFFF +------------------------------+
```

The screen segment is 4 KB aligned because ANTIC playfield DMA cannot cross a
4 KB boundary. The display list and PMG storage are 1 KB aligned for their
respective ANTIC addressing rules. `main()` saves `PORTB`, enables the RAM
beneath BASIC, and restores `PORTB` before returning to DOS.

## Build map

```mermaid
flowchart LR
    PY["gen_trig3d.py<br/>optional"] --> TABLES["trig3d.c / trig3d.h"]
    ASMSRC["four .ASM files"] -->|"ca65 -g"| OBJ["four .o files"]
    CSRC["six .c files"] --> CL65["cl65 -t atari -O -g"]
    TABLES --> CL65
    OBJ --> CL65
    CFG["chased3d.cfg"] --> CL65
    CL65 --> XEX["Chased3D.XEX"]
    CL65 --> LABELS["Chased3D.lbl"]
    CL65 --> LINKMAP["Chased3D.map"]
    CSV["L3.csv, L4.csv, L5.csv"] --> ATR["DOS 2.5 disk image<br/>Chased3D.atr"]
    XEX --> ATR
```