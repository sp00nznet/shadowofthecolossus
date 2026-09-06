#!/bin/sh
# Regenerate everything git-ignored: the lifted PPU tree, the lifted SPU images,
# the import table and the HLE NID table. Run from the repo root. PS3RECOMP
# defaults to the sibling checkout.
#
# Supply your own copy of the game first (see README):
#   python "$PS3RECOMP/tools/pkg_extract.py" pkg/<name>.pkg extracted
#   rpcs3 --decrypt game/EBOOT.BIN
# The retail EBOOT is a retail-keyed NPDRM SELF and needs its RAP; the
# debug-signed EBOOT that ships in the update package decrypts with no key.
set -e
PS3RECOMP="${PS3RECOMP:-../ps3recomp}"
ELF=game/EBOOT.elf

mkdir -p analysis src/recomp src/gen
python "$PS3RECOMP/tools/find_functions.py" "$ELF" --output analysis/functions.json
python "$PS3RECOMP/tools/gen_imports.py"    "$ELF" -o imports.json

# --hle-stubs rewrites each import trampoline as ps3_hle_call(nid), so a direct
# `bl` to an import reaches the HLE handler instead of the literal stub.
#
# --code-end 0x4ECD54 is the end of the last executable section: the 180-entry
# .lib.stub trampoline table at 0x4EB6D4 + 0x1680. It matters more here than on
# the sister ports, because this ELF's PT_LOAD 0 is one 6.9 MB R+X segment
# covering code *and* .rodata. find_functions disassembles the whole thing, and
# its branch-target pass invents 9,642 "functions" out of string tables --
# 0x5701A0 is Shift-JIS dialogue, 0x5B0100 is "MINO_A_WEAPON_ATK_FIX",
# 0x6C32A8 is "/smallfishparam.sidb". Clipping here drops all of them and keeps
# the 15,491 real ones.
rm -rf src/recomp && mkdir -p src/recomp
python "$PS3RECOMP/tools/ppu_lifter.py" "$ELF" \
    --functions analysis/functions.json \
    --hle-stubs imports.json \
    --code-end 0x4ECD54 \
    -o src/recomp

python "$PS3RECOMP/tools/gen_hle_nids.py" --all --out src/gen/ppu_hle_nids.cpp

# ---- SPU -------------------------------------------------------------------
# Eight SPU ELFs are embedded in the EBOOT and extract statically -- no
# SPU_DUMP_MISS capture run needed, unlike Simpsons Arcade. build_spu_workloads
# lifts each under its own C symbol prefix (they all define spu_func_* and
# spu_recomp_register, so they would collide otherwise) and emits
# src/spu_workloads.c, which registers every image with the runtime dispatch
# registry by FNV-1a-64 fingerprint -- the same fingerprint cellSpurs computes
# over the image the title hands it.
python "$PS3RECOMP/tools/extract_spu_images.py" "$ELF" --out analysis/spu
rm -rf src/spu_gen
python "$PS3RECOMP/tools/build_spu_workloads.py" \
    --images analysis/spu --lifted src/spu_gen \
    --out src/spu_workloads.c \
    --register-fn shadow_spu_register_all --constructor --title shadow
