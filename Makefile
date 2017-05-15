CXX = g++

CXXFLAGS = -g `llvm-config --cxxflags` -O0
LDFLAGS = -g `llvm-config --ldflags`

$(shell mkdir -p bin)

all: bin/DirtyPad.so

bin/DirtyPad.so: bin/DirtyPad.o
	$(CXX) $(LDFLAGS) -shared $^ -o $@

bin/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -fPIC -c $< -o $@

check:
	tests/runtests.sh

# For tests
gen-llvm:
	echo '### test1.c'
	clang tests/test1.c -o- -S -emit-llvm -O2 -Xclang -load -Xclang bin/DirtyPad.so
	echo '### test2.cpp'
	clang tests/test2.cpp -o- -S -emit-llvm -O2 -Xclang -load -Xclang bin/DirtyPad.so

clean:
	rm -f bin/*

.PHONY: clean all check
