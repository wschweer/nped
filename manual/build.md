## Build

- create an build directory if not exists alreay:
- enter build directory
- configure by calling cmake
- build the app

```
mkdir build
cd build
cmake -G Ninja ..
cmake --build .
```

or if you want to use the clang compiler:

```
mkdir build
cd build
cmake -D CMAKE_CXX_COMPILER=clang++ -G Ninja ..
cmake --build .
```

usually you need to tell the build system the path of your local Qt installation:

```
export CMAKE_PREFIX_PATH=***YourPath***/Qt/6.11.0/gcc_64
mkdir build
cd build
cmake -D CMAKE_CXX_COMPILER=clang++ -G Ninja ..
cmake --build .
```

start the app

```
./nped ../editor.cpp
```
