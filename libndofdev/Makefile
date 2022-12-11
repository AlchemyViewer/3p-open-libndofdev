CFLAGS := $(CFLAGS) -pipe -Wall -Wunused-result
ifeq ($(USE_SDL2),1)
CFLAGS := $(CFLAGS) -DUSE_SDL2
ifeq ($(DEBUG),1)
LDFLAGS := ${LDFLAGS} -lSDL2d
else
LDFLAGS := ${LDFLAGS} -lSDL2
endif
else
LDFLAGS := ${LDFLAGS} -lSDL
endif


all:		libndofdev.a ndofdev_test

libndofdev.a:	ndofdev.o
		$(AR) -r $@ $<

ndofdev_test:	ndofdev_test.o libndofdev.a
		$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

clean:
		-rm -f *.a *.o ndofdev_test
