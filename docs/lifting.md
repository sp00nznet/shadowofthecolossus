# Lifting NPUA-80677

How the package was identified, decrypted and lifted, and the one flag that
matters. The commands are in [`tools/relift.sh`](../tools/relift.sh); this file
is the reasoning behind them.

## What's in the package

`Shadow Of The Colossus PSN [NPUA-80677]` is a 6.5 GB RAR set holding one retail
package and two update ("crack") packages:

| | |
|---|---|
| **Title** | Shadow of the Colossus |
| **Title ID** | `NPUA80677` |
| **Content ID** | `UP9000-NPUA80677_00-SOTC000000000001` |
| **`PARAM.SFO`** | `CATEGORY=HG`, `APP_VER=01.00`, `PS3_SYSTEM_VER=03.7000` |
| **Package** | `Shadow-Of-The-Colossus_Full.pkg`, 6,474,015,648 B |
| **Payload** | `USRDIR/EBOOT.BIN` (5,090,944 B) — retail NPDRM SELF |
| | `USRDIR/SHADOW/nico.psarc` (6,448,914,007 B) — **all** of the game data |
| | `USRDIR/SHADOW/license.edat` (1,536 B) |
| | `MANUAL/*.DDS` ×10, `TROPDIR/NPWR01503_00/TROPHY.TRP`, icons, `SND0.AT3` |

`nico` is Team ICO's internal name for the engine. The archive is Sony's PSARC,
the same container flOw's PhyreEngine build uses.

## It is a native title, not a PS2 Classic

The working guess was a firmware recompilation like
[tmpsn](https://github.com/sp00nznet/tmpsn). It isn't: `CATEGORY=HG` is an HDD
game, not `2P`, and `USRDIR` holds a real 5 MB PS3 `EBOOT.BIN`. This is
Bluepoint's HD remaster: native PPU/SPU code, `cellGcmSys`, `cellSpurs`, PS3 SDK
3.5.0. The port shape is flOw / Simpsons Arcade.

The PS2 route was checked first, and the finding is worth keeping for whoever
tries a real PS2 Classic: `/dev_flash/ps2emu/ps2_netemu.self` decrypts (retail,
no RAP) to a 10.4 MB PPC64 ELF, 4,409 functions, **zero imports**. It links
everything statically and reaches the kernel through 537 `sc` sites in two
number spaces, one tagged `oris r11,r11,0x8000`. Its region table names
`ee_jit_code`, `eeram_jit_lut` and `ra_trans_code`: **`ps2_netemu` JITs the
Emotion Engine to PowerPC at runtime**, so a static recompiler has nothing to
lift. Not this repo's problem.

## Decrypting the EBOOT

The retail `EBOOT.BIN` is retail-keyed NPDRM and wants
`UP9000-NPUA80677_00-SOTC000000000001.rap`, which this repo does not have and
never ships. The debug-signed EBOOT in the update package decrypts with no key:

```sh
rpcs3 --decrypt game/EBOOT.BIN      # 5,297,872 B SELF -> 21,060,328 B ELF
```

| | |
|---|---|
| Decrypted ELF | 21,060,328 B — PPC64, big-endian, `ET_EXEC` |
| Entry | `0x7148F8` (OPD) |
| `PT_LOAD 0` | R+X @ `0x10000`, `0x6F3FA8` B — code **and** `.rodata` in one segment |
| `PT_LOAD 1` | RW @ `0x710000`, filesz `0xD1282C`, memsz `0x1843440` (~9.6 MB BSS) |
| Functions found | **25,133** (`find_functions.py`) |
| Functions lifted | **16,114** (15,491 real + 602 mid-function tail-entry wrappers) |
| Unhandled instructions | **0** |
| Generated C++ | 101 MB, 1.62 M lines, 3 chunks |
| Imports | **180 across 16 libraries**, 173 named (96%) |
| Embedded SPU ELFs | **8**, 221,340 B, extracted statically |

## Why `--code-end 0x4ECD54`

The gap between 25,133 and 15,491 is the flag. `PT_LOAD 0` is one 6.9 MB R+X
segment covering code and read-only data together, so `find_functions`
disassembles all of it and its branch-target pass invents 9,642 "functions" out
of string tables: `0x5701A0` is Shift-JIS dialogue, `0x5B0100` is
`MINO_A_WEAPON_ATK_FIX`, `0x6C32A8` is `/smallfishparam.sidb`. `0x4ECD54` is
the end of the 180-entry `.lib.stub` trampoline table (`0x4EB6D4 + 0x1680`),
the last executable bytes. Clipping there drops every fake and keeps every real
function. The sister ports needed the same flag for a smaller reason; here it is
38% of the input.

## Imports

| library | n | |
|---|---:|---|
| `cellSpurs` | 33 | tasksets, event flags, LFQueue, barriers |
| `sysPrxForUser` | 29 | CRT, TLS, lwmutex/lwcond |
| `cellGcmSys` | 22 | `_cellGcmInitBody`, display buffers, flip, tiles, Zcull |
| `sys_fs` | 20 | |
| `cellSysutil` | 18 | |
| `cellSpursJq` | 15 | job queue, port2 push/sync |
| `sceNpTrophy` | 9 | |
| `cellFiber` | 6 | PPU fibers |
| `cellAudio` | 6 | |
| `sys_io`, `cellSysmodule`, `sceNp`, `cellGame` | 4 each | |
| `cellRtc` | 3 | |
| `cellSysutilAvconfExt` | 2 | |
| `cellSync` | 1 | |

Graphics need nothing bespoke: the title imports `cellGcmSys`, so ps3recomp's
HLE `cellGcmSys` walks the pushbuffer into `rsx_live_draw_method()` and
`RSX_LIVE_DRAW=1` selects caner's (canersaka) live NV4097 → D3D12 engine. That is
the opposite of Twisted Metal, whose `ps1_netemu` links libgcm statically.

## SPU images

Eight SPU ELFs are embedded in the EBOOT and extract statically, with no
`SPU_DUMP_MISS` capture run (unlike Simpsons Arcade, whose CRI jobs only exist
in memory at dispatch time). Five share entry `0x3050`, the SPURS job2 layout.
Named from their own assertion strings:

| image | size | entry | what it is |
|---|---:|---|---|
| `spu_0001` | 46,496 B | `0x10` | **Edge Geometry** — `JOBCRT Ver13`, culling / blend shapes / skinning |
| `spu_0004` | 67,168 B | `0x3050` | **MultiStream audio** — DSP slots, streams |
| `spu_0005` | 36,512 B | `0x3050` | **Edge Zlib** — `edgeZlibFetchAndInflateLargeRawData` |
| `spu_0003` | 21,024 B | `0x3050` | **`Renderer_SPU`** — `ProcessCommand: %d` |
| `spu_0006` | 34,720 B | `0x3050` | unnamed (stripped) |
| `spu_0007` | 8,716 B | `0x98` | `mstream_dsp_reverb.pic` (GCC 4.1.1 SDK350) |
| `spu_0002` | 5,280 B | `0x3050` | unnamed (stripped) |
| `spu_0000` | 1,424 B | `0x10` | unnamed (stripped) |

`build_spu_workloads.py` lifts each under its own C symbol prefix (they all
define `spu_func_*`, so they would collide otherwise) and emits
`src/spu_workloads.c`, which registers every image by FNV-1a-64 fingerprint, the
same fingerprint `cellSpurs` computes over the image the title hands it.
