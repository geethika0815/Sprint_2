CC = g++
CFLAGS = -c -Wall
LIB = ../lib
all : server client
server : $ server.o 
	${CC} server.o -lpthread -o server
client : $ client.o
	${CC} client.o -lpthread -o client
logger.o: ${LIB}/test.cpp
	${CC} ${CFLAGS} ${LIB}/test.cpp
clean: 
	rm -f *.o server client
