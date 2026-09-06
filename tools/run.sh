#!/bin/sh
# Run the recompiled title against the extracted package.
#
# PS3_VFS_ROOT   -- ppu_fs serves the guest filesystem from here; the game's
#                   own paths are relative to /dev_hdd0/game/NPUA80677/USRDIR.
# RSX_LIVE_DRAW  -- caner's (canersaka) live NV4097 -> D3D12 draw engine.
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"

# Git Bash / MSYS rewrites POSIX-looking paths in environment values on the way
# to a native binary, so a GUEST path like /dev_hdd0/... arrives as
# "C:/Program Files/Git/dev_hdd0/...". These are GUEST paths; leave them alone.
export MSYS2_ENV_CONV_EXCL="*"
# HOST paths must reach the native binary in Windows form, and the blanket
# exclusion above turns MSYS's conversion off for everything -- so convert the
# host ones explicitly rather than relying on a heuristic that has to guess.
ROOT_W="$(cygpath -m "$ROOT")"
export PS3_VFS_ROOT="${PS3_VFS_ROOT:-$ROOT_W/vfs}"
export PS3_HDD0_ROOT="${PS3_HDD0_ROOT:-$ROOT_W/vfs/dev_hdd0}"
export RSX_LIVE_DRAW="${RSX_LIVE_DRAW:-1}"
export PS3_TITLE="${PS3_TITLE:-Shadow of the Colossus - ps3recomp}"

exec "$ROOT/build/shadow.exe" "$ROOT/game/EBOOT.elf" "$@"
