#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>

#include "test.h"

#define END(P, F) ((const unsigned char *)&(P)->F + sizeof((P)->F))
#define PAD(T, F1, F2) (offsetof(T, F2) - offsetof(T, F1) - sizeof(((T *)0)->F1))
#define PAD_LAST(T, F) (sizeof(T) - (unsigned long)&((T *)0)->F - sizeof(((T *)0)->F))

int is_zero(const unsigned char *p, size_t size) {
  for (size_t i = 0; i < size; ++i)
    if (p[i]) return 0;
  return 1;
}

int is_wiped(const unsigned char *p, size_t size) {
  for (size_t i = 0; i < size; ++i)
    if (p[i] != 0xcd) return 0;
  return 1;
}

int main() {
  {
    struct A a = test_return_struct();
    if (!is_wiped(END(&a, c), PAD(struct A, c, s)))
      fprintf(stderr, "test_return_struct: field c not wiped\n");
    if (!is_wiped(END(&a, s), PAD(struct A, s, l)))
      fprintf(stderr, "test_return_struct: field s not wiped\n");
    if (!is_wiped(END(&a, l), PAD_LAST(struct A, l)))
      fprintf(stderr, "test_return_struct: field l not wiped\n");
  }

  {
    struct A *a = test_malloc_struct();
    if (!is_wiped(END(a, c), PAD(struct A, c, s)))
      fprintf(stderr, "test_malloc_struct: field c not wiped\n");
    if (!is_wiped(END(a, s), PAD(struct A, s, l)))
      fprintf(stderr, "test_malloc_struct: field s not wiped\n");
    if (!is_wiped(END(a, l), PAD_LAST(struct A, l)))
      fprintf(stderr, "test_malloc_struct: field l not wiped\n");
  }

  {
    struct A *a = test_new_struct();
    if (!is_wiped(END(a, c), PAD(struct A, c, s)))
      fprintf(stderr, "test_new_struct: field c not wiped\n");
    if (!is_wiped(END(a, s), PAD(struct A, s, l)))
      fprintf(stderr, "test_new_struct: field s not wiped\n");
    if (!is_wiped(END(a, l), PAD_LAST(struct A, l)))
      fprintf(stderr, "test_new_struct: field l not wiped\n");
  }

  {
    void *p = calloc(1, sizeof(struct A));
    struct A *a = test_cast_to_struct(p);
    if (!is_zero(END(a, c), PAD(struct A, c, s)))
      fprintf(stderr, "test_cast_to_struct: field c wiped\n");
    if (!is_zero(END(a, s), PAD(struct A, s, l)))
      fprintf(stderr, "test_cast_to_struct: field s wiped\n");
    if (!is_zero(END(a, l), PAD_LAST(struct A, l)))
      fprintf(stderr, "test_cast_to_struct: field l wiped\n");
  }

  {
    struct A *a = test_placement_new_struct();
    if (!is_wiped(END(a, c), PAD(struct A, c, s)))
      fprintf(stderr, "test_placement_new_struct: field c not wiped\n");
    if (!is_wiped(END(a, s), PAD(struct A, s, l)))
      fprintf(stderr, "test_placement_new_struct: field s not wiped\n");
    if (!is_wiped(END(a, l), PAD_LAST(struct A, l)))
      fprintf(stderr, "test_placement_new_struct: field l not wiped\n");
  }

  {
    struct A *a = test_assign_field();
    if (!is_wiped(END(a, c), PAD(struct A, c, s)))
      fprintf(stderr, "test_field_assign: field c not wiped\n");
    if (!is_wiped(END(a, s), PAD(struct A, s, l)))
      fprintf(stderr, "test_field_assign: field s not wiped\n");
    if (!is_wiped(END(a, l), PAD_LAST(struct A, l)))
      fprintf(stderr, "test_field_assign: field l not wiped\n");
  }

  {
    struct B *b = test_malloc_nested_struct();
    struct A *a = &b->a;
    if (!is_wiped(END(b, c), PAD(struct B, c, a)))
      fprintf(stderr, "test_malloc_nested_struct: field c not wiped\n");
    if (!is_wiped(END(a, c), PAD(struct A, c, s)))
      fprintf(stderr, "test_malloc_nested_struct: field a.c not wiped\n");
    if (!is_wiped(END(a, s), PAD(struct A, s, l)))
      fprintf(stderr, "test_malloc_nested_struct: field a.s not wiped\n");
    if (!is_wiped(END(a, l), PAD_LAST(struct A, l)))
      fprintf(stderr, "test_malloc_nested_struct: field a.l not wiped\n");
  }

  return 0;
}
