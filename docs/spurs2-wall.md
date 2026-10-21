# The SPURS 2.0 wall

Why the title stops before it opens `nico.psarc`, and where the fix belongs.

## The first boot (2026-09-05)

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

The CRT runs, `cellSysmodule` loads eleven modules, `cellGame` boot-checks
against the real `PARAM.SFO`, `sceNpTrophy` creates its context for
`NPWR01503`, GCM initialises, both display buffers register, and the flip loop
turns over ~3,900 frames. It asks `cellSaveData` for `BCUS98174_ICO` and
`BCUS98174_SOTC`, the disc collection's save directories.

**Zero draw packets and zero file opens.** The FIOS workers park forever:

```
wait for invalid cond 'fios worker cond'
wait for invalid cond 'opWait'
```

## The unresolved NIDs

| NID | function | upstream ps3recomp v0.16.0 (2026-10-19) |
|---|---|---|
| `0x30AA96C4` | `cellSpursInitializeWithAttribute2` | missing |
| `0x4A6465E3` | `cellSpursCreateTaskset2` | missing |
| `0xC2ACDF43` | `_cellSpursTasksetAttribute2Initialize` | missing |
| `0x8ADADF65` | `_cellSpursTaskAttribute2Initialize` | missing |
| `0xE14CA62D` | `cellSpursCreateTask2` | missing |
| `0xF244E799` | `_cellSpursCreateJobQueue` | missing |
| `0xCF89F218` | `_cellSpursJobQueuePort2PushJobBody` | missing |
| `0xFACB3CED` | `cellSpursJobQueuePort2Sync` | missing |
| `0x0582338A` `0x0F03F712` `0x1686957E` `0x742CEC0D` `0xE70F874E` `0xFF03CC79` | the six `cellSpursJobQueueAttributeSet*` | missing |
| `0xA3E3BE68` | `sys_ppu_thread_once` | **implemented** (`libs/system/sysPrxForUser.c`); resolves on the next `tools/relift.sh`, not yet re-run |
| `0xFAA275A4` | `cellSysutilAvconfExt` (unnamed) | missing |

The right-hand column was checked by searching `origin/master` of ps3recomp for
each handler name; it is not from a run.

`cellSpursInitializeWithAttribute2` being a stub is why the next call logs
`CreateTaskset REJECT unregistered spurs=0x…`: no SPURS instance is ever
registered, so the taskset the engine's I/O layer needs never exists, so FIOS
has no worker to signal. The sister ports all use SPURS 1.0, which the runtime
implements. This is the first title in the family to need the 2.0 surface.

## Where the fix goes

Upstream, in ps3recomp's `libs/spurs/`. SPURS 2.0 and the Jq Port2 surface are
firmware, not this game: any title built on a 3.x SDK that uses them hits the
same wall. A title-local shim in this repo would be the wrong home (REPO_RULES
"Toolkit changes made during game work").

PR #1 (@Lincoln16yyy, 2026-10-20) proposed a local 2.0 → 1.0 forwarding shim. It
was not merged; the reasons are in the PR. Two of its findings hold up and are
recorded here:

- `sys_ppu_thread_once` is now handled upstream (table above).
- Upstream `cellSpursCreateTasksetWithAttribute` passes `0` for `argTaskset` to
  `spurs_create_taskset`, so a taskset's argument is dropped. That is a
  ps3recomp bug whichever title surfaces it.
