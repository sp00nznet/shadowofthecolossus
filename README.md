# 🗡️🐎 shadowofthecolossus — Static Recompilation

A static recompilation of **Shadow of the Colossus (PS3, NPUA-80677)**, Bluepoint's
HD remaster, into a native Windows executable, built on
[ps3recomp](https://github.com/sp00nznet/ps3recomp). No emulator at runtime.

## Status

**Unreleased, pre-alpha. It boots, opens a window and flips. It draws nothing yet.**

The EBOOT lifts clean (16,114 functions, **0 unhandled instructions**), the eight
embedded SPU modules lift with it, and the first run reaches `cellGcmInit`,
registers both display buffers and runs the flip loop against an empty command
stream. It stalls before it opens `nico.psarc` because ps3recomp does not yet
implement **SPURS 2.0** — see [docs/spurs2-wall.md](docs/spurs2-wall.md).

Last built and run: 2026-09-05, against ps3recomp as of that date. Not yet
rebuilt against ps3recomp v0.16.0 (2026-10-19); the build layout and every
toolkit script `tools/relift.sh` calls are unchanged there.

| Phase | Status |
|---|---|
| Extract, decrypt, find functions, resolve NIDs | ✅ 180 imports, 173 named |
| Lift PPU | ✅ 16,114 functions, 0 unhandled |
| Lift SPU | ✅ 8 images |
| Build | ✅ 45 MB `shadow.exe` |
| First boot | ✅ reaches `cellGcmInit` + flip loop |
| PSARC (`nico.psarc` via HLE `cellFs`) | ⬜ |
| Graphics (live NV4097 → D3D12) | 🟡 engine up, 0 draw packets |
| SPURS (job queue into the lifted images) | ⬜ blocked on SPURS 2.0 upstream |
| Input / audio | ⬜ |

Per-session detail is in [PROGRESS.md](PROGRESS.md).

**Conformance:** this port is not yet registered in ps3recomp's regression gate
(`tools/regress_ports.toml`), so there is no milestone count to report.

## Screenshots

None yet: every frame so far is an empty clear. The first one that draws goes
here.

## Getting Started

You need your own copy of the PSN package. This repository contains no game
code, assets or keys, and never will.

### Quick start

Not available yet. A `Setup.cmd` is on the [roadmap](ROADMAP.md); until then use
the steps below.

### Step by step

Prerequisites: Windows 10/11, Python 3.9+ (`py` if `python` opens the Microsoft
Store), CMake 3.20+, Ninja, LLVM **clang-cl**, an [RPCS3](https://rpcs3.net)
build (only for `--decrypt`), and a sibling
[ps3recomp](https://github.com/sp00nznet/ps3recomp) checkout with its runtime
built (`build/ps3recomp_runtime.lib`).

1. **Unpack the package** into the repo folder:
   ```sh
   python ../ps3recomp/tools/pkg_extract.py pkg/<name>.pkg extracted
   ```
2. **Decrypt the EBOOT.** Use the debug-signed EBOOT from the update package;
   it needs no key (the retail one needs a RAP, which this repo does not use):
   ```sh
   rpcs3 --decrypt game/EBOOT.BIN      # 5,297,872 B SELF -> 21,060,328 B ELF
   ```
3. **Lift** PPU + SPU and generate the HLE NID table (Git Bash):
   ```sh
   PS3RECOMP=../ps3recomp ./tools/relift.sh
   ```
4. **Build.** Release matters: an unoptimised build of 101 MB of generated C++
   measured 3x slower on the sister ports. `llvm-rc` is needed because clang-cl
   outside a VS dev prompt cannot find `rc.exe`.
   ```sh
   cmake -S . -B build -G Ninja \
       -DCMAKE_C_COMPILER="C:/Program Files/LLVM/bin/clang-cl.exe" \
       -DCMAKE_CXX_COMPILER="C:/Program Files/LLVM/bin/clang-cl.exe" \
       -DCMAKE_RC_COMPILER="C:/Program Files/LLVM/bin/llvm-rc.exe"
   cmake --build build
   ```
5. **Point `vfs/PS3_GAME` at the unpacked package** (a directory junction is
   enough; it is 6 GB) and run. Expected today: a window, a flip loop, and the
   FIOS workers parking on `wait for invalid cond 'fios worker cond'`.

The generated source (`src/recomp/`, `src/gen/`, `src/spu_gen/`) lands in
git-ignored directories and is never distributed.

## Usage

```sh
./tools/run.sh    # sets PS3_VFS_ROOT, PS3_HDD0_ROOT, RSX_LIVE_DRAW=1; extra args go to shadow.exe
```

There is no `--headless --record` mode yet; it belongs in ps3recomp's boot
harness and is on the [roadmap](ROADMAP.md).

## Building from source

Steps 3–4 above. `tools/relift.sh` explains its flags inline; the reasoning
behind `--code-end 0x4ECD54`, the decrypt route and the SPU images is in
[docs/lifting.md](docs/lifting.md).

```
  PSN PKG ─► USRDIR/EBOOT.BIN ─► .elf ─► ppu_lifter ─► C++ ─┐
              (NPDRM SELF)    (decrypt)  (16,114 fns)       ├─► link ps3recomp ─► shadow.exe
                              8 SPU ELFs ─► spu_lifter ─►───┘        (harness + HLE)
           USRDIR/SHADOW/nico.psarc (6.0 GiB) ────────────────────► game data (fed, not lifted)
```

## License

[MIT](LICENSE), covering this repository's own tooling, scripts and notes. It
does not cover, and this repository does not distribute, PS3 firmware, game
code, game assets or keys. `pkg/`, `extracted/`, `game/`, `vfs/`, `analysis/`
and the lifted output are git-ignored. There is no RAP, klicensee or key of any
kind in this repository or its history.

## Contributors

See [CONTRIBUTORS.md](CONTRIBUTORS.md). Contributions are welcome under
[CONTRIBUTING.md](CONTRIBUTING.md); please read "Where your code comes from"
first.

## Related projects

This port is one of the [ps3recomp](https://github.com/sp00nznet/ps3recomp) titles
and follows the shared recomp house style. Fixes the runtime needs go upstream there, not here.

- [flOw](https://github.com/sp00nznet/flow) · [simpsonsarcade-ps3](https://github.com/sp00nznet/simpsonsarcade-ps3) — sister ports of the same shape
- [tmpsn](https://github.com/sp00nznet/tmpsn) — Twisted Metal PSOne Classic, the firmware-emulator port this one was first expected to resemble
- [RPCS3](https://github.com/RPCS3/rpcs3) — used here as a black-box tool (`--decrypt`) and as a behavioural oracle; no RPCS3 code is in this repo
