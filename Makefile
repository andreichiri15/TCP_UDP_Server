CC=g++ -std=c++11 -Wall -Wextra

all: build

build: server subscriber

common.o: common.cpp
	$(CC) -c common.cpp

server: server.cpp common.o 
	$(CC) -o server server.cpp common.o

subscriber: subscriber.cpp common.o
	$(CC) -o subscriber subscriber.cpp common.o

clean:
	rm *.o server subscriber