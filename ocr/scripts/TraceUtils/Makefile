CC = gcc

LDFLAGS = -g
LDLIBS =
CFLAGS = -I ../../build/x86 -I ../../src/inc -I ../../inc -I../../src/

all: traceDecode

traceDecode: traceDecode.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

clean:
	rm -f traceDecode

