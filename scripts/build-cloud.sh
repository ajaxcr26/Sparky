#!/usr/bin/env bash
# This environment's verified, workspace-local SDK installation is retained in its snapshot.
# For a fresh Linux machine, use the standard apt/CMake prerequisites in README.md.
set -euo pipefail
cd /workspace/Sparky
export CPLUS_INCLUDE_PATH=/workspace/dependencies/sysroot/usr/include:/workspace/dependencies/sysroot/usr/include/freetype2
export LIBRARY_PATH=/workspace/dependencies/sysroot/usr/lib/x86_64-linux-gnu
export PKG_CONFIG_PATH=/workspace/dependencies/sysroot/usr/lib/x86_64-linux-gnu/pkgconfig:/workspace/dependencies/sysroot/usr/share/pkgconfig
export PKG_CONFIG_SYSROOT_DIR=/workspace/dependencies/sysroot
cmake_bin=/workspace/dependencies/python/cmake/data/bin/cmake
"$cmake_bin" -S . -B build -DFETCHCONTENT_SOURCE_DIR_JUCE=/workspace/dependencies/JUCE -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/workspace/dependencies/sysroot/usr -DCMAKE_CXX_FLAGS=-I/workspace/dependencies/sysroot/usr/include -DCMAKE_EXE_LINKER_FLAGS=-L/workspace/dependencies/sysroot/usr/lib/x86_64-linux-gnu
"$cmake_bin" --build build --parallel 3
/workspace/dependencies/python/cmake/data/bin/ctest --test-dir build --output-on-failure
