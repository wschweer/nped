#!/bin/bash
export PATH=/home/ws/projects/mxe/usr/bin:$PATH
/home/ws/projects/mxe/usr/bin/i686-w64-mingw32.static-cmake -B build-mxe -S . -G Ninja
cmake --build build-mxe

