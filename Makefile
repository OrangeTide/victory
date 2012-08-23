CFLAGS ?= -Wall -W -g -DUSE_SYSLOG=1
#
all :: tests
.PHONY : all clean tests
#
all ::
clean ::
tests ::
#
OBJS_serv := serv.o httpd.o module.o service.o httpparser.o channel.o \
	daemonize.o csv.o mod_static_files.o
all :: serv
clean :: ; $(RM) serv $(OBJS_serv)
serv : $(OBJS_serv)
serv : LDFLAGS += -pthread
#
tests :: test_httpparser ; ./test_httpparser
OBJS_test_httpparser := test_httpparser.o httpparser.o
clean :: ; $(RM) test_httpparser $(OBJS_test_httpparser)
test_httpparser : $(OBJS_test_httpparser)
#
tests :: test_csv ; ./test_csv
OBJS_test_csv := test_csv.o csv.o
clean :: ; $(RM) test_csv $(OBJS_test_csv)
test_csv : $(OBJS_test_csv)
