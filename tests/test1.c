#include <stdlib.h>

#include "test.h"

struct A test_return_struct() {
  struct A a = { 1, 2 };
  return a;
}

struct A *test_malloc_struct() {
  return (struct A *)malloc(sizeof(struct A));
}

struct A *test_cast_to_struct(void *p) {
  return (struct A *)p;
}

struct A *test_assign_field() {
  struct A *p = (struct A *)malloc(sizeof(struct A));
  asm volatile("" :: "m"(p->c));
  p->c = 0;
  asm volatile("" :: "m"(p->s));
  p->s = 0;
  asm volatile("" :: "m"(p->l));
  p->l = 0;
  return p;
}

struct B *test_malloc_nested_struct() {
  return (struct B *)malloc(sizeof(struct B));
}
