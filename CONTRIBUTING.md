# Contributing

Issues and PRs are welcome. Two things decide whether a change lands here or
somewhere else.

## Game repo or toolkit?

This repo holds what is specific to NPUA-80677: build wiring, relift flags,
addresses, notes. Anything another PS3 title could hit (a missing firmware
module, an HLE bug, a lifter gap) belongs in
[ps3recomp](https://github.com/sp00nznet/ps3recomp) as its own PR. Link it from
an issue here and this port will pin the version that has it.

## Where your code comes from

Contributions must be your own work, or come from a source whose licence is
compatible with MIT (MIT, BSD, zlib, Apache-2.0, public domain), with its notice
kept and the source named in the PR.

- **No code written with copyleft source open.** RPCS3 is GPL-2.0, and so are
  most emulators (xemu, Cxbx-Reloaded, PCSX2, Dolphin...). Reading their source
  and then writing "your own" version is derivation, however much it is
  rewritten. Running RPCS3 as a black box and comparing what it outputs is fine,
  and encouraged. A comment like "mirrors RPCS3's implementation" means the PR
  cannot be merged.
- **No Sony SDK material.** No SDK headers, samples or documentation, including
  mirrors of them, and nothing reconstructed from them.
- **Nothing derived from the game binary.** No lifted or decompiled code, no
  disassembly, no dumps, no assets.
- AI-assisted contributions are welcome, provided a person understood and
  tested the change.

## Testing

Say what you ran and what it printed. If you could not run the game, say so; an
untested change to the boot path stays open until someone has run it.
