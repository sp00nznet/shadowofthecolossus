# 🗡️🐎 shadowofthecolossus — Static Recompilation

A static recompilation of **Shadow of the Colossus (PS3, NPUA-80677)** into a native PC
executable — no emulator required — built on
[ps3recomp](https://github.com/sp00nznet/ps3recomp).

> **Status: it boots, opens a window and flips. It draws nothing yet.** The EBOOT
> lifts clean — 16,114 functions, **0 unhandled instructions** — the eight
> embedded SPU modules lift with it, and the first run reaches `cellGcmInit`,
> registers both display buffers and runs the flip loop at ~250 fps against an
> empty command stream. It stalls before it ever opens `nico.psarc`, and the
> reason is named below. Everything here is what the binary actually did, not
> what it is hoped to do.

---

## 📺 What's in the box

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

Three files in `USRDIR`. One of them is 6.0 GiB. `nico` is Team ICO's internal
name for the engine, and the archive is Sony's PSARC — the same container flOw's
PhyreEngine build uses, so the runtime already knows how to open it.

## 🎯 The recompilation target — and the guess that was wrong

The working assumption going in was that this would be another *firmware*
recompilation, the way [Twisted Metal PSN](https://github.com/sp00nznet/tmpsn)
turned out to be: a PSOne Classic is a disc image in a wrapper, and the thing
that runs it is `ps1_netemu.self` off the console's own `dev_flash`. A PS2
Classic would be the same trick one console generation up.

**It isn't.** `CATEGORY=HG` is an HDD game, not `2P`, and `USRDIR` holds a real
5 MB PS3 `EBOOT.BIN`. `NPUA80677` is the standalone PSN release of Bluepoint
Games' **HD remaster** — native PPU/SPU code, `cellGcmSys`, `cellSpurs`,
PS3 SDK 3.5.0 — so this is an ordinary port in the shape of
[flOw](https://github.com/sp00nznet/flow) and
[Simpsons Arcade](https://github.com/sp00nznet/simpsonsarcade-ps3), not of
Twisted Metal.

The PS2 route was checked before being ruled out, and the note is worth keeping
for whoever tries an actual PS2 Classic:
`/dev_flash/ps2emu/ps2_netemu.self` decrypts (retail, no RAP) to a 10.4 MB PPC64
ELF, 4,409 functions, **zero imports** — it links everything statically and
reaches the kernel through 537 `sc` sites in two distinct spaces, one of them
tagged `oris r11,r11,0x8000`. Its own region table names `rsx_driver_info`,
`rsx_dma_ctrl` and `rsx_reports`, so the sys_rsx tap tmpsn built would apply. But
its region table also names `ee_jit_code`, `eeram_jit_lut` and `ra_trans_code`:
**`ps2_netemu` JITs the Emotion Engine to PowerPC at runtime.** A static
recompiler has nothing to lift there — the code the PS2 game runs does not exist
until the emulator writes it. That is a different and much harder problem than
the PS1 emulator's interpreter-on-SPU, and it is not this repository's problem.

### The EBOOT

The retail `EBOOT.BIN` is retail-keyed NPDRM and wants
`UP9000-NPUA80677_00-SOTC000000000001.rap`, which we do not have and do not
ship. The debug-signed EBOOT in the update package decrypts with no key at all:

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

**The gap between 25,133 and 15,491 is the interesting number.** This ELF's
`PT_LOAD 0` is one 6.9 MB R+X segment covering code and read-only data together,
so `find_functions` disassembles the lot and its branch-target pass invents 9,642
"functions" out of string tables — `0x5701A0` is Shift-JIS dialogue, `0x5B0100`
is `MINO_A_WEAPON_ATK_FIX`, `0x6C32A8` is `/smallfishparam.sidb`. Clipping the
lift at `--code-end 0x4ECD54`, the end of the 180-entry `.lib.stub` trampoline
table, drops every one of them and keeps every real function. The sister ports
needed the same flag for a smaller reason; here it is 38% of the input.

## 📚 What it imports

| library | n | |
|---|---:|---|
| `cellSpurs` | 33 | tasksets, event flags, LFQueue, barriers |
| `sysPrxForUser` | 29 | CRT, TLS, lwmutex/lwcond |
| **`cellGcmSys`** | **22** | `_cellGcmInitBody`, `cellGcmSetDisplayBuffer`, `cellGcmSetPrepareFlip`, `cellGcmGetControlRegister`, tiles, Zcull |
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

## 🖥️ Graphics

Nothing bespoke is needed. The title imports `cellGcmSys` — 22 functions,
including `_cellGcmInitBody` and the whole flip path — which is exactly the tap
flOw, Simpsons Arcade and You Don't Know Jack render through: ps3recomp's HLE
`cellGcmSys` walks the pushbuffer and feeds `rsx_live_draw_method()`, and
`RSX_LIVE_DRAW=1` selects caner's (canersaka) live NV4097 → D3D12 engine.

That is the opposite of Twisted Metal's problem. `ps1_netemu` imports no
`cellGcmSys` at all — it links libgcm statically and goes through twelve
`sys_rsx_*` syscalls, so that port had to move the tap down a layer into
`libs/video/sys_rsx.c`. Here the ordinary path applies unchanged.

## 🧩 The SPU side

Eight SPU ELFs are embedded in the EBOOT and extract statically — no
`SPU_DUMP_MISS` capture run needed, unlike Simpsons Arcade, whose CRI jobs only
exist in main memory at dispatch time. Five share entry `0x3050`, the SPURS
job2 layout. Named from their own assertion strings:

| image | size | entry | what it is |
|---|---:|---|---|
| `spu_0001` | 46,496 B | `0x10` | **Edge Geometry** — `JOBCRT Ver13`, culling / blend shapes / skinning uniform tables |
| `spu_0004` | 67,168 B | `0x3050` | **MultiStream audio** — DSP slots, streams, domain checks |
| `spu_0005` | 36,512 B | `0x3050` | **Edge Zlib** — `edgeZlibFetchAndInflateLargeRawData`, inflate |
| `spu_0003` | 21,024 B | `0x3050` | **`Renderer_SPU`** — `ProcessCommand: %d` |
| `spu_0006` | 34,720 B | `0x3050` | unnamed (stripped) |
| `spu_0007` | 8,716 B | `0x98` | `mstream_dsp_reverb.pic` (`.note.spu_name`, GCC 4.1.1 SDK350) |
| `spu_0002` | 5,280 B | `0x3050` | unnamed (stripped) |
| `spu_0000` | 1,424 B | `0x10` | unnamed (stripped) |

`tools/relift.sh` lifts each under its own C symbol prefix and emits
`src/spu_workloads.c`, which registers all eight with the runtime dispatch
registry by FNV-1a-64 fingerprint — the same fingerprint `cellSpurs` computes
over the image the title hands it.

## 🚦 First boot — how far it gets, and what stops it

One 90-second run, `RSX_LIVE_DRAW=1`, no patches:

```
[ppu] loaded 2 PT_LOAD segments, entry OPD 0x007148F8
[cellGame] BootCheck: type=1, titleId='NPUA80677'
[SPU] initialize(nspu=6, nrawspu=0)
[HLE] _cellGcmInitBody(cmdSize=0x200000, ioSize=0x3400000, ioAddr=0x40300000)
[rsx] live-draw engine up (D3D12); GDI present suppressed
[live-draw] display buffer 0 = loc0:0x00000000 pitch=... 
[cellSaveData] AutoLoad2(version=..., dir='BCUS98174_SOTC')
[SYS] sys_ppu_thread_create name="fios scheduler 0" ...
[live-draw] frame 3936 packets[seen=0 queued=0] groups[seen=0 exec=0] ...
```

So: the recompiled CRT runs, TLS is set up, `cellSysmodule` loads eleven
modules, `cellGame` boot-checks against the real `PARAM.SFO`, `sceNpTrophy`
creates its context for `NPWR01503`, GCM initialises, the live-draw engine binds
its swap chain, both display buffers register, tiles and Zcull bind, and the flip
loop turns over ~3,900 frames in 90 s. It also asks `cellSaveData` for
`BCUS98174_ICO` and `BCUS98174_SOTC` — the disc collection's save directories,
still in the PSN build.

**Zero draw packets, and zero file opens.** It never reads `nico.psarc`. The
FIOS worker threads park forever on two conditions the runtime never created:

```
wait for invalid cond 'fios worker cond'
wait for invalid cond 'opWait'
```

Every unresolved NID in the run points at the same gap — **SPURS 2.0 and the
job-queue module are not implemented in the HLE layer**:

| NID | function |
|---|---|
| `0x30AA96C4` | `cellSpursInitializeWithAttribute2` |
| `0x4A6465E3` | `cellSpursCreateTaskset2` |
| `0xC2ACDF43` | `_cellSpursTasksetAttribute2Initialize` |
| `0x8ADADF65` | `_cellSpursTaskAttribute2Initialize` |
| `0xE14CA62D` | `cellSpursCreateTask2` |
| `0xF244E799` | `_cellSpursCreateJobQueue` |
| `0xCF89F218` | `_cellSpursJobQueuePort2PushJobBody` |
| `0xFACB3CED` | `cellSpursJobQueuePort2Sync` |
| `0x0582338A` `0x0F03F712` `0x1686957E` `0x742CEC0D` `0xE70F874E` `0xFF03CC79` | the six `cellSpursJobQueueAttributeSet*` |
| `0xA3E3BE68` | `sys_ppu_thread_once` |
| `0xFAA275A4` | `cellSysutilAvconfExt` (unnamed) |

`cellSpursInitializeWithAttribute2` being a stub is why the very next call logs
`CreateTaskset REJECT unregistered spurs=0x…`: nothing ever registered a SPURS
instance, so the taskset the engine's I/O layer needs never exists, so FIOS has
no worker to signal. That single import is the whole wall. The sister ports all
used SPURS 1.0 (`cellSpursInitializeWithAttribute`, `CreateTaskset`), which the
runtime does implement — this title is the first to need the 2.0 surface.

## 🛠️ Pipeline

```
  PSN PKG ─► USRDIR/EBOOT.BIN ─► .elf ─► ppu_lifter ─► C++ ─┐
              (NPDRM SELF)    (decrypt)  (16,114 fns)       ├─► link ps3recomp ─► shadow.exe
                              8 SPU ELFs ─► spu_lifter ─►───┘        (harness + HLE)
                                                                            │
           USRDIR/SHADOW/nico.psarc (6.0 GiB) ────────────────────► game data (fed, not lifted)
```

## 📦 Building

Prereqs: Python 3.9+, CMake 3.20+, **clang-cl** + Ninja, and a sibling
[ps3recomp](https://github.com/sp00nznet/ps3recomp) checkout.

```bash
# 1. Supply your own copy of the game, then unpack and decrypt:
python ../ps3recomp/tools/pkg_extract.py pkg/<name>.pkg extracted
rpcs3 --decrypt game/EBOOT.BIN

# 2. Lift PPU + SPU and generate the HLE NID table:
PS3RECOMP=../ps3recomp ./tools/relift.sh

# 3. Build (Release matters -- an unoptimised build of a 101 MB generated
#    translation unit measured 3x slower on the sister ports).
#    llvm-rc: clang-cl outside a VS dev prompt cannot find Microsoft's rc.exe.
cmake -S . -B build -G Ninja \
    -DCMAKE_C_COMPILER="C:/Program Files/LLVM/bin/clang-cl.exe" \
    -DCMAKE_CXX_COMPILER="C:/Program Files/LLVM/bin/clang-cl.exe" \
    -DCMAKE_RC_COMPILER="C:/Program Files/LLVM/bin/llvm-rc.exe"
cmake --build build

# 4. Run with the live NV4097 -> D3D12 draw engine. tools/run.sh sets
#    PS3_VFS_ROOT and RSX_LIVE_DRAW=1; point vfs/PS3_GAME at your unpacked
#    package (a directory junction is enough -- it is 6 GB).
./tools/run.sh
```

## ⚖️ Legal

This repository distributes **no game code, no assets, no firmware, no
encryption keys** — only analysis notes, configuration and recompilation
tooling. You must supply your own legally obtained copy of the game. `pkg/`,
`extracted/`, `game/`, `vfs/`, `analysis/` and the lifted output (`src/recomp/`,
`src/spu_gen/`, `src/gen/`) are git-ignored.

There is no RAP, no klicensee and no key of any kind in this repository or its
history — nothing that decrypts anything.

Licence: [MIT](LICENSE), covering this repository's own tooling, scripts and notes.

## 🔗 Related Projects

- [ps3recomp](https://github.com/sp00nznet/ps3recomp) — the PS3 HLE runtime this links against
- [flOw](https://github.com/sp00nznet/flow) · [simpsonsarcade-ps3](https://github.com/sp00nznet/simpsonsarcade-ps3) · [youdontknowjack](https://github.com/sp00nznet/youdontknowjack) — the sister ports that render
- [tmpsn](https://github.com/sp00nznet/tmpsn) — Twisted Metal PSOne Classic, the firmware-emulator port this one was expected to resemble
- [RPCS3](https://github.com/RPCS3/rpcs3) — emulator whose HLE research (and `--decrypt`) makes this possible
