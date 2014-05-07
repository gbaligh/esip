#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>


#include <CUnit/CUnit.h>
#include <CUnit/TestDB.h>
#include <CUnit/Basic.h>


extern CU_SuiteInfo    osip_tests_suites[];

extern CU_SuiteInfo    ev_tests_suites[];

/**
 * main
 * @brief The main function (first executed function)
 *
 * @return 0 in success, or positive value for erreur.
 */
int main(int argc, char * argv[])
{
  CU_pSuite       pSuite = NULL;
  int             i = 0;

  /* initialize the CUnit test registry */
  if (CUE_SUCCESS != CU_initialize_registry()) {
    return CU_get_error();
  }

  if (CUE_SUCCESS != CU_register_suites(osip_tests_suites)) {
    return CU_get_error();
  }

  if (CUE_SUCCESS != CU_register_suites(ev_tests_suites)) {
    return CU_get_error();
  }

  /* Run all tests using the CUnit Basic interface */
  CU_basic_set_mode(CU_BRM_VERBOSE);

  CU_basic_run_tests();
  CU_cleanup_registry();
  return CU_get_error();
}
