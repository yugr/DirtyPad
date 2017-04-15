#ifndef TEST_H
#define TEST_H

#ifdef __cplusplus
extern "C" {
#endif

struct A {
  char c;
  short s;
  long l;
};

struct A test_return_struct();

struct A *test_malloc_struct();

struct A *test_cast_to_struct(void *p);

struct A *test_assign_field();

struct A *test_new_struct();

struct A *test_placement_new_struct();

struct B {
  char c;
  struct A a;
};

struct B *test_malloc_nested_struct();

#ifdef __cplusplus
}
#endif

#endif  // TEST_H
