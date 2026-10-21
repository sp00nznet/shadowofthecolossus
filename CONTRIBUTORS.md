# Contributors

## Maintainer

- **sp00nz** ([@sp00nznet](https://github.com/sp00nznet)): the port.

## Issue reports and testing

- **[@Lincoln16yyy](https://github.com/Lincoln16yyy)**: PR #1 (2026-10-20). It
  mapped all 15 unresolved first-boot NIDs onto the SPURS 2.0 / Jq Port2 /
  avconf surface and noticed three things this port had missed:
  `sys_ppu_thread_once` is already implemented upstream, so it only needs a
  relift; upstream `cellSpursCreateTasksetWithAttribute` drops the taskset
  argument; and `_cellSpursCreateJobQueue` takes ten arguments, so two of them
  are read from the guest stack and need a ctx handler. The shim itself was not
  merged (see the PR), but those findings are recorded in
  [docs/spurs2-wall.md](docs/spurs2-wall.md).
