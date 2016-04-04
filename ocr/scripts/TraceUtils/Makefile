CC = gcc

LDFLAGS = -g
LDLIBS =
CFLAGS = -I ../../build/x86 -I ../../src/inc -I ../../inc -I../../src/

all: traceDecode

traceDecode: traceDecode.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o traceDecode $<

128: traceDecode.c
	$(CC) -DENABLE_128_BIT_GUID $(CFLAGS) $(LDFLAGS) -o traceDecode $<

clean:
	rm -f traceDecode

