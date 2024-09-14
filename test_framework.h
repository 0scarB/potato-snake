// NOTE: We don't need an include guard (yet) becuase we only have one snake.c file
#include <stdbool.h>

void test_begin(char* group_description);
void test_end(void);
void test_assert(bool condition, char* fmt, ...);
void test_assert_strs_n_eq(char* left, char* right, int n);
int  test_report_returning_exit_status(void);
