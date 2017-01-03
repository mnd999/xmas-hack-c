CC=clang
LIBS=-lwebsockets -ljansson -lm
PREFIX=/usr/local
LIBDIR=${PREFIX}/lib
INCDIR=${PREFIX}/include


xmashack: xmas-hack.c
	$(CC) xmas-hack.c -o xmashack -g -I$(INCDIR) -L$(LIBDIR)  $(LIBS)

clean:
	rm xmashack
