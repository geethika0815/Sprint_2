CC = g++
CFLAGS = -c -Wall
LIB = ../lib
all : server client
server : $ server.o
	${CC} server.o -lpthread -o server
client : $ client.o
	${CC} client.o -lpthread -o client
clean:
	rm -f *.o server client
