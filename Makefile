CFLAGS ?= -Wall -W -g
#
all ::
.PHONY : all clean
#
%.so : %.o
	$(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -shared -o $@ $<
#TODO: -Wl,...
#
all :: serv
OBJS_serv := serv.o channel.o httpd.o httpparser.o daemonize.o
clean :: ; $(RM) serv $(OBJS_serv)
serv : $(OBJS_serv)
SERV_CFLAGS += -fPIC
SERV_LIBS +=
#
all :: test_httpparser
OBJS_test_httpparser := test_httpparser.o httpparser.o
clean :: ; $(RM) test_httpparser $(OBJS_test_httpparser)
test_httpparser : $(OBJS_test_httpparser)
