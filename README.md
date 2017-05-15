# What's this?

DirtyPad is a proof-of-concept tool which tries to provoke errors
on read overflows in structure fields (something that existing tools
e.g. AddressSanitizer are not capable of).

The idea is to fill struct pads with garbage whenever
structure is created (in static memory,
on stack or in dynamic memory). This would cause
read overflows to return garbage (rather than usual zero)
and hopefully cause crashes.

# How to build

To build in Ubuntu, install llvm and `make` as usual.
To test, run `make check`.

# How to run

Add `-Xclang -load -Xclang path/to/DirtyPad.so` to `CFLAGS`
and `CXXFLAGS`. Or alternatively, set `CC` and `CXX` to
wrappers in `scripts/` folder.

# Results

Tbd. Currently I mainly see this failing in packages which
do `memcmp` on structs (e.g. libsndfile) which is a bad
but not-so-critical coding practice.
