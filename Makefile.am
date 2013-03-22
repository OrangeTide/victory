bin_PROGRAMS = victory serv

# TODO: let configure detect these
AM_CFLAGS = -Wall -W -g
# AM_CFLAGS += -DUSE_SYSLOG=1

victory_SOURCES = victory.c channel.c net.c csv2.c buffer.c
victory_CFLAGS = -pthread
victory_LDFLAGS = -pthread
victory_LDADD = -ldl

serv_SOURCES = serv.c httpd.c module.c service.c httpparser.c channel.c \
	daemonize.c csv.c net.c env.c util.c ext.c \
	mod_static_files.c mod_counter.c
serv_CFLAGS = -pthread
serv_LDFLAGS = -pthread

# Unit tests
check_PROGRAMS = test_httpparser test_csv test_env test_util
TESTS = $(check_PROGRAMS)
test_httpparser_SOURCES = test_httpparser.c httpparser.c
test_csv_SOURCES = test_csv.c csv.c
test_env_SOURCES = test_env.c env.c
test_util_SOURCES = test_util.c util.c
