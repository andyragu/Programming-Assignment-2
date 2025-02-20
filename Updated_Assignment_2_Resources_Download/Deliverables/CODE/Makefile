CC = gcc
CFLAGS = -Wall -pthread -lrt

all: server client

server: server.c
	$(CC) $(CFLAGS) server.c -o server -lrt -pthread

client: client.c
	$(CC) $(CFLAGS) client.c -o client -lrt -pthread

clean:
	rm -f server client
