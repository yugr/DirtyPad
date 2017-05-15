#!/bin/sh -eu

CFLAGS='-g -O2'
PLUGIN='-Xclang -load -Xclang ../bin/DirtyPad.so'

cd $(dirname $0)

mkdir -p bin

for OPT in -O0 -O2; do
  echo "Testing $OPT..."

  rm -f bin/*
  clang $CFLAGS driver.c -c -o bin/driver.o
  for f in test*.c; do
    clang $CFLAGS $PLUGIN $f -c -o bin/$f.o
  done
  for f in test*.cpp; do
    clang++ $CFLAGS $PLUGIN $f -c -o bin/$f.o
  done
  clang $CFLAGS bin/*.o -o bin/test -lstdc++

  bin/test
done
