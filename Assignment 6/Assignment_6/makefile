all:	server client 

server: 	udparq_server.c
	gcc -o udpserver -std=c99 udparq_server.c -w
client:		udparq_client.cpp
	g++ -o udpclient -std=c++11 udparq_client.cpp -w
clean:
	-rm udpserver udpclient
build:
	make clean
	make all
 	
