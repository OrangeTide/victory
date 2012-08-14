CFLAGS ?= -Wall -W -g
#
all ::
.PHONY : all clean
#
all :: serv
OBJS_serv := serv.o httpd.o service.o httpparser.o channel.o daemonize.o
clean :: ; $(RM) serv $(OBJS_serv)
serv : $(OBJS_serv)
serv : LDFLAGS += -pthread
#
all :: test_httpparser
OBJS_test_httpparser := test_httpparser.o httpparser.o
clean :: ; $(RM) test_httpparser $(OBJS_test_httpparser)
test_httpparser : $(OBJS_test_httpparser)
