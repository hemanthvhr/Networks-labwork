/* 
 * udpclient.c - A simple UDP client
 * usage: udpclient <host> <port>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <fcntl.h>

#define BUFSIZE 1074	//1024 + 50
#define HASHSIZE 32

int chunk = 1024;	//size of file chunks

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

int sendf(struct sockaddr* serveraddr,int serverlen,int sockfd,char *file,int size) {
	int fd = open(file,O_RDONLY);
	int seq = 0;
	char buff[BUFSIZE],buf[BUFSIZE],ack[5];
	int comm[2];
	pipe(comm);	//pipe for implementing a time out
	while(size>0) {
		int chk = chunk;
		if(size < chunk) chk = size;
		size -= chk;
		bzero(buff,BUFSIZE);

		int d[4];
		int m = chk;
		for(int i=0;i<4;i++) {
			d[i] = m%10;
			m = m/10;
		}
		
		sprintf(buff,"000%d%d%d%d%d",seq,d[3],d[2],d[1],d[0]);	//Preparing packet header
		bzero(buf,BUFSIZE);

		int n = read(fd,buf,chk);
		if(n!=chk) error("Not able to read characters");
		strcat(buff,buf);	//Packet is Ready in buff

		int flag = 0;//ACK packet not received
		socklen_t len;
	
		while(!flag) {
			sendto(sockfd,buff,chk+8,0,serveraddr,serverlen);
			/*temp*/
			//sleep(1);
			len = sizeof(serveraddr);
			int n = recvfrom(sockfd, buf, 4,0,(struct sockaddr *) serveraddr, &len);
			if(n!=4) continue;
			buf[4] = '\0';
			printf("ACK has been received - %s\n",buf);
			sprintf(ack,"ACK%d",seq);
			if(strcmp(ack,buf)==0) flag = 1;
			/*temp*/
			/*Timeout implementation*/
			//recvfrom(buf,4);if(strcmp(buf,"ACK<seq>")==0) flag = 1;
		}
		seq = 1 - seq;		
	}
	close(fd);
	return 1;
}

int main(int argc, char **argv) {
    int sockfd, portno, n;
    int serverlen;
    struct sockaddr_in serveraddr;
    struct hostent *server;
    char *hostname;
    char buf[BUFSIZE];

    /* check command line arguments */
    if (argc != 3) {
       fprintf(stderr,"usage: %s <hostname> <port>\n", argv[0]);
       exit(0);
    }
    hostname = argv[1];
    portno = atoi(argv[2]);

    /* socket: create the socket */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");

    /* gethostbyname: get the server's DNS entry */
	
	struct timeval tv;
	tv.tv_sec = 1;	//one second timeout
	tv.tv_usec = 0;	
	if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv)) < 0) {
		error("Setting the timeout");
	}
	
    server = gethostbyname(hostname);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host as %s\n", hostname);
        exit(0);
    }

    /* build the server's Internet address */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, 
	  (char *)&serveraddr.sin_addr.s_addr, server->h_length);
    serveraddr.sin_port = htons(portno);

	//Entering the details of the file	
	int filesize = 0;    
	char file[400],*hash;
    printf("Enter the name of the file - ");
	scanf("%s",file);
	int fd = open(file,O_RDONLY);
	if(fd == -1) error("ERROR");
	filesize = fsize(file);
	hash = hashsum(file);

	int chnks = filesize/chunk + 1; //no of chunks of the file
	
	/* constructing a hello message*/
    bzero(buf, BUFSIZE);
 	sprintf(buf,"Hello %s %d %d",file,filesize,chnks);

     /* send the message to the server */
    serverlen = sizeof(serveraddr);
    n = sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr *) &serveraddr, serverlen);
    if (n < 0) 
      error("ERROR in sendto");
    
    /* print the server's reply */
    n = recvfrom(sockfd, buf, strlen(buf), 0, (struct sockaddr *) &serveraddr, &serverlen);
    if (n < 0) 
      error("ERROR in recvfrom");
    printf("Echo from server: %s", buf);

	// starting the file transfer
	printf("\nStarting the File Transfer\n");
	sendf((struct sockaddr *) &serveraddr,serverlen,sockfd,file,filesize);
	printf("\nFile Transfer operation Complete\n");

	/* print the server Hash reply */
    n = recvfrom(sockfd, buf, HASHSIZE, 0, (struct sockaddr *) &serveraddr, &serverlen);
    if (n < 0) 
      error("ERROR in recvfrom");
    printf("Echo from server: %s\n", buf);	
	printf("Original Hash is %s\n",hash);
	/*checking the received hash*/	
	int flag = 0;
	if(!strcmp(hash,buf)) flag = 1;	//hashes matched	
	
	/* Acknowledge to the server about receveing the hash*/
    serverlen = sizeof(serveraddr);
	bzero(buf,BUFSIZE);
	sprintf(buf,"ACK OK");
	n = sendto(sockfd, buf, strlen(buf), 0,(struct sockaddr *) &serveraddr, serverlen);
	if (n < 0) 
	  error("ERROR in sendto");
	printf("\nMD5 sum ");
	if(!flag) printf("not ");
	printf("matched\n"); 
	return 0;
}





