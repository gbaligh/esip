#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <CUnit/CUnit.h>
#include <CUnit/TestDB.h>
#include <CUnit/Basic.h>


#include <event2/event.h>

#define UNUSED(x) (void(x))

static struct event_base * ev = NULL;

static void test_nop(void)
{
  printf("\n\t%d: GCC Version: %s", __COUNTER__, __VERSION__);
  printf("\n\t%d: date: %s", __COUNTER__, __DATE__);
  printf("\n\t%d: time: %s", __COUNTER__, __TIME__);

  printf("\n\t%d: base file: %s", __COUNTER__, __BASE_FILE__);
  printf("\n\t%d: include level: %d", __COUNTER__, __INCLUDE_LEVEL__);
  CU_PASS("nothing");
}

static void test_supported_methods(void)
{
  int i = 0;
  const char ** methods = event_get_supported_methods();
  printf("list:[");
  for (i=0; methods[i] != NULL; ++i) {
    if (i>0) {
      printf(",");
    }
    printf("%s", methods[i]);
  }
  printf("]");
  printf(",used:[%s]... ", event_base_get_method(ev));
}

static CU_TestInfo     all_tools_test[] = {
  {"Supported methods", test_supported_methods},
  {"Test9", test_nop},

  CU_TEST_INFO_NULL,
};

static int init_suite()
{
  printf("\n\n===========================================================\n");
  printf("Libevent v%s.", event_get_version());
  printf("\n===========================================================\n");
  ev = event_base_new();
  return (ev == NULL);
}

static int clean_suite()
{
  event_base_free(ev);
  ev = NULL;
  return 0;
}

CU_SuiteInfo    ev_tests_suites[] = {
  {"LibEvent Tests", init_suite, clean_suite, all_tools_test},

  CU_SUITE_INFO_NULL,
};
