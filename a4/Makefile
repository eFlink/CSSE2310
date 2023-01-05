CC = gcc
CFLAGS = -std=gnu99 -Wall -pedantic -pthread -I/local/courses/csse2310/include
DEBUG = -g
INCLUDE = -L/local/courses/csse2310/lib
A3 = -lcsse2310a3
A4 = -lcsse2310a4
STRING = -lstringstore
LIBCFLAGS = -fPIC -Wall -pedantic -std=gnu99

.PHONY: all clean

all: dbclient dbserver libstringstore.so

dbclient: dbclient.c
	$(CC) $(CFLAGS) $(INCLUDE) $(A4) dbclient.c -o dbclient

dbserver: dbserver.c
	$(CC) $(CFLAGS) $(INCLUDE) $(STRING) $(A3) $(A4) dbserver.c -o dbserver

stringstore.o: stringstore.c
	$(CC) $(LIBCFLAGS) -c $<

libstringstore.so: stringstore.o
	$(CC) -shared -o $@ stringstore.o
