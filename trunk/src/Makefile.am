AUTOMAKE_OPTIONS = foreign subdir-objects

bin_PROGRAMS = esip
AM_CPPFLAGS = -g -Wall 
AM_CFLAGS = -g -Wall -O

esip_SOURCES = esip.c eslog.c cli/escli.c sip/esomsg.c sip/estransport.c sip/esosip.c
esip_LDADD =  $(OSIP2_LIBS) $(LIBEVENT_LIBS) -lcli -lpthread
esip_CFLAGS = $(OSIP2_CFLAGS) $(LIBEVENT_CFLAGS)
