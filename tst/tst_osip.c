#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <osipparser2/osip_port.h>
#include <osipparser2/osip_parser.h>
#include <osipparser2/sdp_message.h>

#include <osip2/osip.h>
#include <osip2/osip_mt.h>
#include <osip2/osip_fifo.h>
#include <osip2/osip_condv.h>
#include <osip2/osip_time.h>

#include <CUnit/CUnit.h>
#include <CUnit/TestDB.h>
#include <CUnit/Basic.h>

#define UNUSED(x) (void(x))

static osip_message_t *sip = NULL;
static osip_t         *osip = NULL;

static int init_suite_osip_msg(void)
{
	parser_init();
	osip_message_init(&sip);

	return (sip == NULL);
}

static int clean_suite_osip_msg(void)
{
	osip_message_free(sip);
	return 0;
}

static int init_suite_osip(void)
{
	printf("\n\n===========================================================\n");
	printf("OSip2.");
	printf("\n===========================================================\n");

	osip_init(&osip);
	return (osip == NULL);
}

static int clean_suite_osip(void)
{
	osip_release(osip);
	return 0;
}

static void test_nop(void)
{
	CU_PASS("nothing");
}

static void test_osip_msg(void)
{
	CU_ASSERT_FATAL(sip != NULL);
}

static void test_osip_msg_parse(void)
{
	const char     *msg = "REGISTER sip:bell-tel.com SIP/2.0\r\n"
		"Via: SIP/2.0/UDP saturn.bell-tel.com\r\n"
		"From: <sip:watson@bell-tel.com>;tag=19\r\n"
		"To: sip:watson@bell-tel.com\r\n"
		"Call-ID: 70710@saturn.bell-tel.com\r\n"
		"CSeq: 2 REGISTER\r\n" "Expires: 0\r\n" "Contact: *\r\n\r\n";
	{
		size_t          len = strlen(msg);
		CU_ASSERT_FATAL(osip_message_parse(sip, msg, len) == 0);
		osip_message_force_update(sip);
		{
			size_t          length = 0;
			char           *result = NULL;
			CU_ASSERT_FATAL(osip_message_to_str(sip, &result, &length) != -1)
				CU_PASS("Message Parsed");
			osip_free(result);
		}
	}
}

static void test_osip_init()
{
	CU_ASSERT_FATAL(osip != NULL);
}

static void * _my_thread(void *arg)
{
	while (arg != (void *) 0)
	{
		// run server
	}
	osip_thread_exit();
	return NULL;
}

static void test_osip_thread()
{
	struct osip_thread *thread = NULL;
	thread = osip_thread_create(2000, _my_thread, (void *) 0);
	CU_ASSERT_FATAL(thread != NULL);
	CU_ASSERT(osip_thread_join(thread) == 0);
	osip_build_random_number();
	osip_free(thread);
}

static void test_osip_random(void)
{
	unsigned int    _random1 = osip_build_random_number();
	unsigned int    _random2 = osip_build_random_number();
	CU_ASSERT(_random1 != _random2);
	CU_ASSERT(osip_build_random_number() != osip_build_random_number());
	CU_ASSERT(osip_build_random_number() != _random1);
	CU_ASSERT(osip_build_random_number() != _random2);
}

static CU_TestInfo     all_msg_test[] =
{
	{"Initializing msg", test_osip_msg},
	{"Parsing SIP msg", test_osip_msg_parse},
	{"Test10", test_nop},

	CU_TEST_INFO_NULL,
};

static CU_TestInfo     all_stk_test[] =
{
	{"Initializing stack", test_osip_init},
	{"Create thread", test_osip_thread},
	{"Generate Random Number", test_osip_random},
	{"Test9", test_nop},

	CU_TEST_INFO_NULL,
};

static CU_TestInfo     all_tools_test[] =
{
	{"Test1", test_nop},
	{"Test9", test_nop},

	CU_TEST_INFO_NULL,
};

CU_SuiteInfo    osip_tests_suites[] =
{
	{"OSIP2 Tools Tests", NULL, NULL, all_tools_test},
	{"OSIP2 Parser Tests", init_suite_osip_msg, clean_suite_osip_msg, all_msg_test},
	{"OSIP2 Tests", init_suite_osip, clean_suite_osip, all_stk_test},

	CU_SUITE_INFO_NULL,
};
