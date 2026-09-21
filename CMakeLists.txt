# Shadow of the Colossus HD (NPUA-80677, Bluepoint Games / Team ICO) — PPU boot harness.
#
# This is a *native* PS3 title, not a firmware emulator wrapper: the package
# carries a real USRDIR/EBOOT.BIN plus one 6.0 GB PSARC of game data. So the
# shape is flOw / Simpsons Arcade, not Twisted Metal PSN — load the real EBOOT
# -> register the lifted functions + HLE NID handlers -> run real boot code, and
# every unlifted function / unimplemented import / lv2 syscall logs itself.
#
# Graphics come from the harness: the title imports cellGcmSys (22 functions),
# so RSX_LIVE_DRAW=1 selects caner's (canersaka) live NV4097 -> D3D12 draw
# engine with no code of our own. See README.
#
# Toolchain: clang-cl (the harness uses __builtin_bswap + weak symbols MSVC lacks).
#   cmake -S . -B build -G Ninja -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl
#   cmake --build build
#   ./tools/run.sh
cmake_minimum_required(VERSION 3.20)

# Default to Release. A recompiled title is one enormous generated translation
# unit -- 25,133 functions here -- and CMake+Ninja pick Debug when no type is
# given, which means no optimisation and no inlining on every frame. The sister
# ports measured 3x slower that way, and it is invisible unless you read
# build.ninja, because everything still works.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "build type" FORCE)
endif()
message(STATUS "build type: ${CMAKE_BUILD_TYPE}")

project(shadow CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD 17)

set(PS3RECOMP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../ps3recomp" CACHE PATH "ps3recomp repo root")
set(PS3RECOMP_RUNTIME_LIB "${PS3RECOMP_DIR}/build/ps3recomp_runtime.lib"
    CACHE FILEPATH "prebuilt runtime library")

# Lifted game code + the generated HLE NID table (both git-ignored, regenerate
# with tools/relift.sh).
file(GLOB RECOMP_SOURCES "src/recomp/*.cpp")
file(GLOB GEN_SOURCES    "src/gen/*.cpp" "src/gen/*.c")
# Lifted SPURS job images, once a SPU_DUMP_MISS capture has produced them. The
# title drives cellSpursJobQueue hard (15 imports), so this will not stay empty.
file(GLOB_RECURSE SPU_SOURCES "src/spu_gen/*.c")
file(GLOB WORKLOAD_SOURCES "src/spu_workloads.c")
if(NOT RECOMP_SOURCES)
    message(FATAL_ERROR "src/recomp/ is empty — run tools/relift.sh first")
endif()
if(NOT SPU_SOURCES)
    message(WARNING "src/spu_gen/ is empty -- SPURS jobs will not dispatch. "
                    "Capture them with SPU_DUMP_MISS, see tools/relift.sh")
endif()

add_executable(shadow
    "${PS3RECOMP_DIR}/runtime/ppu/tests/boot_main.cpp"   # generic boot harness
    "${PS3RECOMP_DIR}/runtime/ppu/ppu_loader.cpp"        # vm + ELF loader + run
    "${PS3RECOMP_DIR}/runtime/ppu/ppu_hle.cpp"           # NID -> handler dispatch
    "${PS3RECOMP_DIR}/runtime/ppu/ppu_sysprx.cpp"        # boot-critical CRT
    "${PS3RECOMP_DIR}/runtime/ppu/ppu_fs.cpp"            # cellFs VFS
    ${WORKLOAD_SOURCES}
    ${GEN_SOURCES}
    ${SPU_SOURCES}
    ${RECOMP_SOURCES}
)

# Every libs/* subdir (the generated NID table #includes each module's header).
file(GLOB LIB_INC_DIRS LIST_DIRECTORIES true "${PS3RECOMP_DIR}/libs/*")

target_include_directories(shadow PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src/compat"            # <dirent.h>/<unistd.h> Win shims
    "${CMAKE_CURRENT_SOURCE_DIR}/src/recomp"            # ppu_recomp.h
    "${PS3RECOMP_DIR}/runtime/ppu"
    "${PS3RECOMP_DIR}/runtime/spu"
    "${PS3RECOMP_DIR}/runtime/prx"
    "${PS3RECOMP_DIR}/include"
    ${LIB_INC_DIRS}
)

target_link_libraries(shadow PRIVATE "${PS3RECOMP_RUNTIME_LIB}")
if(WIN32)
    target_link_libraries(shadow PRIVATE ws2_32 bcrypt xinput ole32 d3d12 dxgi dxguid d3dcompiler)
    # Every guest call is a host call, so recompiled call chains nest deeply.
    target_link_options(shadow PRIVATE /STACK:268435456 /MAP:shadow.map)
endif()
