# Changelog

All notable changes to this port. Format: [Keep a Changelog](https://keepachangelog.com/en/1.1.0/);
versions follow SemVer, tagged `vX.Y.Z`.

## [Unreleased]

### Added
- Scaffold: CMake build against ps3recomp, `tools/relift.sh` (PPU + 8 SPU
  images + HLE NID table), `tools/run.sh`. First boot reaches `cellGcmInit` and
  the flip loop; stalls on SPURS 2.0 (2026-09-05).
- `docs/lifting.md` and `docs/spurs2-wall.md`, moved out of the README.
- `CHANGELOG.md`, `ROADMAP.md`, `CONTRIBUTING.md`, `CONTRIBUTORS.md`.

### Changed
- README reordered into the house sections; status now states the ps3recomp
  version it was last built against.
- `LICENSE` is plain MIT again, so GitHub detects it; the scope notes moved to
  the README's License section.
- `.gitignore` also covers `*.rap`, `build-farm/`, `build-gate/`, `work/`.
- `docs/spurs2-wall.md` records that `sys_ppu_thread_once` is now handled
  upstream, and the upstream `argTaskset` gap, both found in PR #1 —
  *[@Lincoln16yyy](https://github.com/Lincoln16yyy)* (#1).
