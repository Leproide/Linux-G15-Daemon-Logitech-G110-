
METASOURCES = AUTO
AM_CFLAGS = -DG15DAEMON_BUILD -Wall
sbin_PROGRAMS = g15daemon
noinst_PROGRAMS = g15daemontest
noinst_HEADERS = g15logo.h
g15daemon_SOURCES = utility_funcs.c g15daemon.h main.c linked_lists.c g15_plugins.c
g15daemon_LDADD = -ldl
g15daemon_LDFLAGS = -rdynamic
g15daemontest_SOURCES = lcdclient_test.c
INCLUDES = -I$(top_builddir)/libg15daemon_client/
g15daemontest_LDADD = $(top_builddir)/libg15daemon_client/libg15daemon_client.la
include_HEADERS = g15daemon.h
