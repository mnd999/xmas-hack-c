CC=clang
LIBS=-lwebsockets -ljansson
LIBDIR=/usr/local/lib
INCDIR=/usr/local/include


xmashack: xmas-hack.c
	$(CC) xmas-hack.c -o xmashack -g -I$(INCDIR) -L$(LIBDIR)  $(LIBS)

clean:
	rm xmashack
