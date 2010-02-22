TARGETS = ppstest

CFLAGS += -Wall -O2 -D_GNU_SOURCE
CFLAGS += -I . -I ../../include/
CFLAGS += -ggdb

# -- Actions section --

.PHONY : all depend dep

all : .depend $(TARGETS)

.depend depend dep :
	$(CC) $(CFLAGS) -M $(TARGETS:=.c) > .depend

ifeq (.depend,$(wildcard .depend))
include .depend
endif


# -- Clean section --

.PHONY : clean

clean :
	rm -f *.o *~ core .depend
	rm -f ${TARGETS}
