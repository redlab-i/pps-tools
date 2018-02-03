TARGETS = ppstest ppsctl ppswatch ppsldisc

CFLAGS += -Wall -O2 -D_GNU_SOURCE
CFLAGS += -ggdb
CFLAGS += -fPIC
LDLIBS += -lm

# -- Actions section --

.PHONY : all depend dep

all : .depend $(TARGETS)

.depend depend dep :
	$(CC) $(CFLAGS) -M $(TARGETS:=.c) > .depend

ifeq (.depend,$(wildcard .depend))
include .depend
endif

install : all
	install -m 755 -t $(DESTDIR)/usr/bin ppsfind $(TARGETS)
	install -m 644 -t $(DESTDIR)/usr/include/sys timepps.h

uninstall :
	for f in $(TARGETS); do rm $(DESTDIR)/usr/bin/$$f; done
	rm $(DESTDIR)/usr/include/sys/timepps.h

# -- Clean section --

.PHONY : clean

clean :
	rm -f *.o *~ core .depend
	rm -f ${TARGETS}
