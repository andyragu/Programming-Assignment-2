CC = gcc
CFLAGS = -Wall -pthread -lrt

all: server client1 client2

server: server.c
	$(CC) $(CFLAGS) server.c -o server -lrt -pthread

client1: client1.c
	$(CC) $(CFLAGS) client1.c -o client1 -lrt -pthread

client2: client2.c
	$(CC) $(CFLAGS) client2.c -o client2 -lrt -pthread

clean:
	rm -f server client1 client2
