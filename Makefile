all:
	cd build;make;
	/usr/local/opt/llvm/bin/clang -S -emit-llvm -Xclang -disable-O0-optnone foo.c
	/usr/local/opt/llvm/bin/opt -mem2reg -load build/skeleton/libSkeletonPass.* -skeleton -S foo.ll -o foo2.ll

