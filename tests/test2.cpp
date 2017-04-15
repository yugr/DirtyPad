#include <stdlib.h>
#include <new>

#include "test.h"

// TODO: non-trivial structs/classes too?

extern "C" struct A *test_new_struct() {
  return new A;
}

extern "C" struct A *test_placement_new_struct() {
  char *p = (char *)malloc(sizeof(A));
  return new(p) A;
}
