/* 
 * udpserver.c - A UDP echo server 
 * usage: udpserver <port_for_server>
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>

#define BUFSIZE 1074

/*
 * error - wrapper for perror
 */
void error(char *msg) {
  perror(msg);
  exit(1);
}

char *hashsum(char * file) {
	char *md5 = (char *) malloc(240*sizeof(char));
	md5[0]='m';md5[1]='d';md5[2]='5';md5[3]='s';md5[4]='u';md5[5]='m';md5[6]=' ';md5[7]='\0';
	char *hash= (char *) malloc(33*sizeof(char));
	strcat(md5,file);
	FILE* fd = popen(md5,"r");
	fscanf(fd,"%s",hash);
	fclose(fd);	
	free(md5);
	return hash;
}

int fsize(char * file) {
	FILE *fp = fopen(file,"r");
	fseek(fp,0,2);
	int n = ftell(fp);
	fclose(fp);
	return n;
}

int readf(struct sockaddr_in* clientaddr,int clientlen,int sockfd,char *file,int size,int chunks) {
	FILE * fp = fopen(file,"w");
	char buf[BUFSIZE],ack[5];
	int i = 0,tot=0,seq = 0;//initial sequence number
	socklen_t len;
	while(i < chunks) {
		bzero(buf,BUFSIZE);	
		len = sizeof(clientaddr);
		int n = recvfrom(sockfd, buf, BUFSIZE, 0,(struct sockaddr *) clientaddr, &len);
		if(buf[3]-'0' != seq) continue;	//thats not the correct packet
		sprintf(ack,"ACK%d",seq);
		//if(i==0) printf("first recv - %s\n",buf);
		if(sendto(sockfd,ack,strlen(ack),0,(struct sockaddr *) clientaddr,clientlen)) 	//sending te acknowledgement ACK<seq>
			printf("ACk has been sent\n");		
		tot += (n-8);
		fprintf(fp,"%s",buf+8);	//write the data into the file to reconstruct it
		seq = 1 - seq;
		i++;
	}
	fclose(fp);
	printf("total - %d\n",tot);
	if(tot==size) return 1;
	else return 0;
}
int main(int argc, char **argv) {
  int sockfd; /* socket file descriptor - an ID to uniquely identify a socket by the application program */
  int portno; /* port to listen on */
  socklen_t clientlen; /* byte size of client's address */
  struct sockaddr_in serveraddr; /* server's addr */
  struct sockaddr_in clientaddr; /* client addr */
  struct hostent *hostp; /* client host info */
  char buf[BUFSIZE]; /* message buf */
  char *hostaddrp; /* dotted decimal host addr string */
  int optval; /* flag value for setsockopt */
  int n; /* message byte size */

  /* 
   * check command line arguments 
   */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port_for_server>\n", argv[0]);
    exit(1);
  }
  portno = atoi(argv[1]);

  /* 
   * socket: create the socket 
   */
  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0) 
    error("ERROR opening socket");

  /* setsockopt: Handy debugging trick that lets 
   * us rerun the server immediately after we kill it; 
   * otherwise we have to wait about 20 secs. 
   * Eliminates "ERROR on binding: Address already in use" error. 
   */
  optval = 1;
  setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, 
	     (const void *)&optval , sizeof(int));

  /*
   * build the server's Internet address
   */
  bzero((char *) &serveraddr, sizeof(serveraddr));
  serveraddr.sin_family = AF_INET;
  serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
  serveraddr.sin_port = htons((unsigned short)portno);

  /* 
   * bind: associate the parent socket with a port 
   */
  if (bind(sockfd, (struct sockaddr *) &serveraddr, 
	   sizeof(serveraddr)) < 0) 
    error("ERROR on binding");

  /* 
   * main loop: wait for a datagram, then echo it
   */
  clientlen = sizeof(clientaddr);
  while (1) {

    /*
     * recvfrom: receive a UDP datagram from a client
     */
    bzero(buf, BUFSIZE);
    n = recvfrom(sockfd, buf, BUFSIZE, 0,
		 (struct sockaddr *) &clientaddr, &clientlen);
    if (n < 0)
      error("ERROR in recvfrom");

    /* 
     * gethostbyaddr: determine who sent the datagram
     */
    hostp = gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr, 
			  sizeof(clientaddr.sin_addr.s_addr), AF_INET);
    if (hostp == NULL)
      error("ERROR on gethostbyaddr");
    hostaddrp = inet_ntoa(clientaddr.sin_addr);
    if (hostaddrp == NULL)
      error("ERROR on inet_ntoa\n");
    printf("server received datagram from %s (%s)\n", 
	   hostp->h_name, hostaddrp);
    printf("server received %zd/%d bytes: %s\n", strlen(buf), n, buf);
	int filesize,chunks;
	char *file;
	//Read the message from the client
	strtok(buf," ");
	file = strtok(NULL," ");
	filesize = atoi(strtok(NULL," "));
	chunks = atoi(strtok(NULL," "));
	
	printf("OK file - %s filename - %d chunks - %d\n",file,filesize,chunks);

	//send a reply to the client

	sprintf(buf,"Hello");
	n = sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr *) &clientaddr, clientlen);
	if (n < 0) 
		error("ERROR in sendto");
	char actfile[] = "some_random.txt";
	
	
	//start receiving the file
	if(readf(&clientaddr,clientlen,sockfd,actfile,filesize,chunks)) printf("\nReceived the file correctly\n");

	/*Calculating the hash and sending it to the client*/
	char *hash = hashsum(actfile);
	for(int i=0;i<10;i++) {	//spamming the client with hash to make sure it receives it
		n = sendto(sockfd, hash, strlen(hash), 0, (struct sockaddr *) &clientaddr, clientlen);
		if (n < 0) 
			error("ERROR in sendto for hashsum");
	}	
	char ack[7] = "ACK OK";
	//waiting for the acknowledgement of the hashsum
	int i = 10;
	sleep(3);
	while(i--) {
		clientlen = sizeof(clientaddr);
		int n = recvfrom(sockfd, buf, 6, MSG_DONTWAIT,(struct sockaddr *) &clientaddr, &clientlen);
		buf[6] = '\0';
		if(n!=6) continue;
		if(strcmp(ack,buf)==0) break;
	}
	if(i==-1) printf("ACK has not been received\n");
	else printf("ACK has been received - %s\n",buf);
  }
}
