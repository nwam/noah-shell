CC = gcc
CFLAGS = -c -g -O0 -std=gnu11

all: ns.o
	$(CC) -o ns ns.o

ns.o: ns.c
	$(CC) $(CFLAGS) ns.c
	
clean:
	rm -rf *.o
