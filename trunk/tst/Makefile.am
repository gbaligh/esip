testdir=${datadir}/@PACKAGE@/Test

test_PROGRAMS = test udpclient clitest
AM_CPPFLAGS = 
AM_CFLAGS = -g -Wall 

test_SOURCES = tst.c tst_osip.c tst_ev.c
test_LDADD = -lcunit $(OSIP2_LIBS) $(LIBEVENT_LIBS)
test_CFLAGS = $(OSIP2_CFLAGS) $(LIBEVENT_CFLAGS)

udpclient_SOURCES = udpclient.c

clitest_SOURCES = clitest.c
clitest_LDADD = -lcli
