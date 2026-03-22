#
#     this helper file defines several project tasks as
#     "make" dependencies for easy command line usage
#

export LOGFILE=/home/ws/nped/.nped.log

nped:
	#export QT_FATAL_WARNINGS=true
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
