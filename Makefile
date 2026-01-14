CC = gcc
CFLAGS = -shared -fPIC -Wall -Werror -pedantic -std=gnu99
LFLAGS = 
IFLAGS = 


malloc: libmalloc.a libmalloc.so

libmalloc.so: malloc64.o
	$(CC) -shared -o $@ malloc64.o $(LFLAGS)

libmalloc.a: malloc64.o
	ar r $@ $<
	

intel-all: lib/libmalloc.so lib64/libmalloc.so

lib/libmalloc.so: lib malloc32.o
	$(CC) -m32 -shared -o $@ malloc32.o $(LFLAGS)

lib64/libmalloc.so: lib64 malloc64.o
	$(CC) -shared -o $@ malloc64.o $(LFLAGS)

lib:
	mkdir lib

lib64:
	mkdir lib64

malloc32.o: malloc.c
	$(CC) $(CFLAGS) $(IFLAGS) -m32 -c -o malloc32.o malloc.c

malloc64.o: malloc.c
	$(CC) $(CFLAGS) $(IFLAGS) -m64 -c -o malloc64.o malloc.c

.PHONY: clean malloc

clean:
	rm -rf $(EXECUTABLE) *.o lib/ lib64/ *.so *.a
