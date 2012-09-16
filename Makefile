CFLAGS ?= -Wall -W -g
#
all :: tests
.PHONY : all clean tests
#
all ::
clean ::
tests ::
#
OBJS_victory := victory.o
all :: victory
clean :: ; $(RM) victory $(OBJS_victory)
victory : $(OBJS_victory)
victory : CFLAGS += -pthread
victory : LDFLAGS += -pthread
