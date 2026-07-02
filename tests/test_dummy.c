#include <stdio.h>
#include <stdlib.h>

#define ASSERT(expr, message)                                                  \
  do {                                                                         \
    if (!(expr)) {                                                             \
      (void)fprintf(stderr, "FAIL: %s (%s:%d)\n", message, __FILE__,           \
                    __LINE__);                                                 \
      exit(1);                                                                 \
    }                                                                          \
  } while (0)

void test_placeholder(void) { ASSERT(1 == 1, "1 should equal 1"); }

int main(void) {
  printf("Running dummy C tests...\n");
  test_placeholder();
  printf("All C tests passed!\n");
  return 0;
}
