# Progress Log — shadowofthecolossus

## Phase overview

| Phase | Description | Status |
|---|---|---|
| 0. Recon | Identify the package; test the "firmware emulator" hypothesis | ✅ — wrong, it is a native PS3 title |
| 1. Extract | Unpack PSN PKG → EBOOT.BIN + `nico.psarc` | ✅ |
| 2. Decrypt | EBOOT.BIN (SELF) → EBOOT.elf | ✅ — debug-signed update EBOOT, no RAP needed |
| 3. Disasm/find | OPD + heuristic function discovery | ✅ — 25,133 raw, 15,491 real |
| 4. NID resolve | Import table → library/function names | ✅ — 16 libs, 180 funcs, 173 named |
| 5. Lift PPU | ppu_lifter → C++ | ✅ — 16,114 functions, 0 unhandled |
| 6. Lift SPU | 8 embedded SPU ELFs → C + workload registry | ✅ |
| 7. Build | clang-cl + Ninja against ps3recomp | ✅ — 45 MB `shadow.exe` |
| 8. First boot | Enter the recompiled CRT | ✅ — reaches cellGcmInit + flip loop |
| 9. PSARC | Open `nico.psarc` through the HLE `cellFs` VFS | ⬜ |
| 10. Graphics | RSX → D3D12 via the live NV4097 engine | 🟡 — engine up, window open, 0 draw packets |
| 11. SPURS | Job queue dispatch into the 8 lifted images | ⬜ — **blocked**: SPURS 2.0 + cellSpursJq unimplemented |
| 12. Input / audio | cellPad, cellAudio | ⬜ |

## Detailed log

### 2026-09-05 — Recon, and the hypothesis that did not survive

The working guess was that `NPUA-80677` would be a PS2 Classic, i.e. the same
shape as [tmpsn](https://github.com/sp00nznet/tmpsn): a disc image in a wrapper,
recompile the firmware emulator rather than the game.

Checked the PS2 route first. `/dev_flash/ps2emu/ps2_netemu.self` decrypts with
`rpcs3 --decrypt` (retail, no RAP) to a 10,442,536 B PPC64 ELF: 3 PT_LOADs, entry
`0x14`, text at vaddr 0, BSS of 0x2653A78 (~38 MB). `find_functions` gives 4,409
functions. `gen_imports` gives **zero** — no `.lib.stub` at all, everything
linked statically. It reaches the kernel through 537 `sc` sites in two distinct
number spaces: a plain `li r11,N; sc` space (0–20 plus 141) and a second one
tagged `li r11,N; oris r11,r11,0x8000; sc` (sparse, 0–249). No `sys_rsx_*`
(668–681) in either, but its own memory-region name table has `rsx_driver_info`,
`rsx_dma_ctrl`, `rsx_reports`, `rsx_audio_info` — so the tap tmpsn built into
`libs/video/sys_rsx.c` would be the place to look.

The blocker for that route is elsewhere in the same table: `ee_jit_code`,
`eeram_jit_lut`, `eerom_jit_lut`, `ra_trans_code`. **`ps2_netemu` JITs the
Emotion Engine to PowerPC at runtime.** Nothing to lift statically; the code the
PS2 game executes does not exist until the emulator writes it. Different problem,
not this one. (It also carries its own PS2 ROM — `eeload.conf`,
`PS20220WD20050620.conf` — a full USB/HID/Bluetooth stack, a TrueType engine, a
VU microcode disassembler, and an interactive "SELECT IMAGE TO BOOT" menu.)

Then the package itself settled it. `PARAM.SFO`:

```
CATEGORY          HG                  <- HDD game, not 2P
TITLE_ID          NPUA80677
PS3_SYSTEM_VER    03.7000
TITLE             Shadow of the Colossus
```

`USRDIR` holds `EBOOT.BIN` (5,090,944 B), `SHADOW/nico.psarc` (6,448,914,007 B)
and `SHADOW/license.edat`. Three files. It is Bluepoint's HD remaster, native
PS3 code, and the port shape is flOw / Simpsons Arcade.

### 2026-09-05 — Decrypt, lift, build

The retail EBOOT wants `UP9000-NPUA80677_00-SOTC000000000001.rap`. The
debug-signed EBOOT in the update package (`Shadow-Of-The-Colossus_Crack.pkg`,
5,297,872 B) decrypts with no key: 21,060,328 B ELF, entry `0x7148F8`.

`find_functions` reports 25,133. Only 15,491 of those are real. `PT_LOAD 0` is
one 6.9 MB R+X segment holding code *and* `.rodata`, so the branch-target pass
walks straight off the end of the last executable section and manufactures 9,642
functions out of string tables — spot-checked `0x5701A0` (Shift-JIS dialogue),
`0x5B0100` (`MINO_A_WEAPON_ATK_FIX`), `0x6C32A8` (`/smallfishparam.sidb`).
`--code-end 0x4ECD54` — the end of the 180-entry `.lib.stub` table at
`0x4EB6D4 + 0x1680` — cuts exactly at the boundary.

Lift: 16,114 functions (15,491 + 602 mid-function tail-entry wrappers), 6,077
unique call targets, **0 unhandled instructions**, 101 MB of C++ over 3 chunks.

SPU: unlike Simpsons Arcade, the job images are real ELFs embedded in the EBOOT,
so `extract_spu_images.py` finds all 8 statically (221,340 B). Named from their
own assertion strings: Edge Geometry (`JOBCRT Ver13`), Edge Zlib, MultiStream
audio DSP, `Renderer_SPU`, `mstream_dsp_reverb.pic`, and three stripped. Five
share entry `0x3050`, the SPURS job2 layout. `build_spu_workloads.py` lifts each
under its own symbol prefix and emits the fingerprint registry.

Build: clang-cl + Ninja, Release, 19 targets, `build/shadow.exe` at 45,368,320 B.

### 2026-09-05 — First boot

Ran cold, `RSX_LIVE_DRAW=1`, 90 s, no patches. It gets a long way:

- recompiled CRT runs, `sys_initialize_tls` sets `r13`, `sys_memory` serves the
  1 MB / 2 MB / 54 MB allocations
- `cellSysmodule` loads eleven modules (FIBER, AUDIO, FS, SPURS, GCM_SYS, IO,
  RTC, NP, NP_TROPHY, AVCONF_EXT, GAME)
- `cellGame` `BootCheck` against the real `PARAM.SFO` → `NPUA80677`,
  `ContentPermit` → `/dev_bdvd/PS3_GAME`
- `sceNpTrophy` `CreateContext(commId="NPWR01503")`
- `SPU initialize(nspu=6, nrawspu=0)`
- `_cellGcmInitBody(cmdSize=0x200000, ioSize=0x3400000, ioAddr=0x40300000)`,
  then `SetDisplayBuffer` ×2, `SetTile`, `BindTile`, `BindZcull`,
  `SetFlipHandler`, `SetVBlankHandler`, `SetFlipMode`
- live-draw engine up on D3D12, window open, ~3,900 flips in 90 s
- `cellSaveData AutoLoad2` for `BCUS98174_ICO` and `BCUS98174_SOTC` — the disc
  collection's save dirs, still present in the PSN build
- FIOS spins up `fios scheduler 0` and `fios mediathread N`

And then stops. **0 draw packets, 0 file opens** — `nico.psarc` is never
touched. The FIOS workers park on `wait for invalid cond 'fios worker cond'`
and `'opWait'` forever.

Root cause is one import. Every unresolved NID in the run is SPURS 2.0 or the
job-queue module: `cellSpursInitializeWithAttribute2` (0x30AA96C4),
`cellSpursCreateTaskset2` (0x4A6465E3), `cellSpursCreateTask2` (0xE14CA62D),
both `*Attribute2Initialize`, `_cellSpursCreateJobQueue` (0xF244E799),
`_cellSpursJobQueuePort2PushJobBody` (0xCF89F218),
`cellSpursJobQueuePort2Sync` (0xFACB3CED), the six
`cellSpursJobQueueAttributeSet*`, plus `sys_ppu_thread_once` (0xA3E3BE68) and
one unnamed `cellSysutilAvconfExt` NID. With
`cellSpursInitializeWithAttribute2` stubbed, nothing registers a SPURS
instance, so the next call logs `CreateTaskset REJECT unregistered spurs=0x…`,
so the engine's I/O taskset never exists, so FIOS has no worker to signal.

The sister ports all use SPURS 1.0, which ps3recomp implements. This is the
first title in the family to need the 2.0 surface. That is the next job, and it
belongs upstream in ps3recomp, not here.

**Next:** implement `cellSpursInitializeWithAttribute2` + `CreateTaskset2` in
ps3recomp's `cellSpurs`, then `cellSpursJq`, then re-run and see whether FIOS
opens the PSARC.
