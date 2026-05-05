#
#     this helper file defines several project tasks as
#     "make" dependencies for easy command line usage
#

export LOGFILE=/home/ws/nped/.nped.log

nped:
	#export QT_FATAL_WARNINGS=true; cd build; cmake --build . --parallel 32 && cd .. && build/nped
	cd build; cmake --build . --parallel 32 && cd .. && build/nped

#
#     "test" target
#
t:
	./build/nped

#
#     "debug" target
#
d:
	gdb build/nped core*

#
#     "install" target
#
i:
	cp build/nped  /home/ws/bin

#
#     push
#
push:
	git rebase -i origin/main

#
#     "valgrind" target
#
v:
	cd build; ninja -j32 && cd ..; valgrind --tool=memcheck --trace-children=yes build/nped editor.cpp editor.h

#
#     initialize the project to use the gcc compiler
#
init-gcc:
	rm -rf build; mkdir build; cd build; cmake -G Ninja ..

#
#     initialize the project to use the clang compiler
#
init:
	rm -rf build; mkdir build; cd build; cmake -D CMAKE_CXX_COMPILER=clang++ -G Ninja ..

#
#     initialize the project for cross compiling to windows using mxe
#
init-windows:
	rm -rf build-windows; mkdir build-windows; cd build-windows; /home/ws/projects/mxe/usr/bin/i686-w64-mingw32.static-cmake -G Ninja ..

init-windows-shared:
	rm -rf build-windows-shared; mkdir build-windows-shared; cd build-windows-shared; /home/ws/projects/mxe/usr/bin/x86_64-w64-mingw32.shared-cmake -G Ninja ..
