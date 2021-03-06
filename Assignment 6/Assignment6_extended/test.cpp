/* 
 * udpclient.c - A simple UDP client
 * usage: udpclient <portno> <host> <port>
 */
#include "tcpwrapper.h"
#include <cstdio>
#include <iostream>
#include <string.h>

using namespace std;

int main(int argc,char **argv) {
	/* check command line arguments */
    if (argc != 4) {
       fprintf(stderr,"usage: %s <portno> <hostname> <port>\n", argv[0]);
       exit(0);
    }
	cout << "Ok\n";
	init_send(atoi(argv[1]),argv[2],atoi(argv[3]));
	cout << "Ok2\n";
	char buf[BUFSIZE];
	bzero(buf,BUFSIZE);
	int i=0;
	while(i>6) i += apprecv(buf,BUFSIZE);
	while(!R.empty()) {
		cout << R.front() << "\n";
		R.pop();
	}
	while(1);
	return 0;
}