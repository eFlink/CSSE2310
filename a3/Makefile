CC = gcc
CFLAGS = -Wall -pedantic -std=gnu99
TARGETS = sigcat hq
CSSE2310 = -I/local/courses/csse2310/include -L/local/courses/csse2310/lib -lcsse2310a3

all: $(TARGETS)

sigcat: sigcat.c
	$(CC) $(CFLAGS) -o sigcat sigcat.c

hq: hq.c
	$(CC) $(CFLAGS) $(CSSE2310) -o hq hq.c
