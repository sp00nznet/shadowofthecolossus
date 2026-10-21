# Roadmap

## Next

1. **SPURS 2.0 and Jq Port2 in ps3recomp** (`libs/spurs/`): the 14 missing
   NIDs in [docs/spurs2-wall.md](docs/spurs2-wall.md), as a toolkit PR, written
   from documentation and black-box RPCS3 comparison only. Then relift, rebuild
   and see whether FIOS opens `nico.psarc`.
2. **Upstream `argTaskset` fix**: `cellSpursCreateTasksetWithAttribute` drops
   the taskset argument.
3. **Rebuild against ps3recomp v0.16.0** and refresh the first-boot log.
4. **Register in ps3recomp's regression gate** (`tools/regress_ports.toml`) and
   record a golden, so the README can report a milestone count.
5. **netlab recipe** (`projects/shadowofthecolossus.env`) so builds stop running
   on the workstation.

## Deferred

- `Setup.cmd` quick start. Waits until the port draws something; a one-click
  setup that ends at an empty window helps nobody.
- `--headless --record out.mp4`. Belongs in ps3recomp's boot harness, shared by
  every port, not here.
- `cellSysutilAvconfExt` NID `0xFAA275A4`: name and behaviour unconfirmed.
- Input and audio.
- CI: nothing here builds without the game's data, so a push workflow would
  have nothing to run.

## Out of scope

- PS2 Classic support (`ps2_netemu` JITs the EE; see docs/lifting.md).
- Distributing the game, its assets, generated source or any key.
