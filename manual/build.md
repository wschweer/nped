## Build

### Prerequisites

- A C++23 compiler: `clang++` (preferred) or `g++`
- Qt 6 (>= 6.5) with the modules: Core, Gui, Widgets, Network, WebEngineCore,
  WebEngineWidgets, WebChannel, DBus, PrintSupport and Concurrent
- libgit2 (Git integration)
- md4c and md4c-html (Markdown rendering)
- FFmpeg libraries: libavcodec, libavformat, libavutil, libswscale
- `clangd` (language server for C/C++, used at runtime)
- Bubblewrap (`bwrap`) for sandboxing the AI agent's shell commands

### Build

- create a build directory if not already exists:

```
mkdir build
cd build
```

- configure by calling cmake (tell cmake the path of your Qt installation if it is
  not found automatically):

```
export CMAKE_PREFIX_PATH=***YourPath***/Qt/6.x.x/gcc_64
cmake -D CMAKE_CXX_COMPILER=clang++ -G Ninja ..
```

or if you want to use the gcc compiler:

```
cmake -G Ninja ..
```

- build the app

```
cmake --build . --parallel 32
```

There is a helper `Makefile` with convenience targets:

```
make nped    # build and run
make t       # run the built binary
make d       # debug with gdb
make v       # valgrind
make init    # wipe build dir and configure with clang
```

- start the app (one or more files must be given)

```
./nped ../src/editor.cpp
```
