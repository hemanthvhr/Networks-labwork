/* 
 * tcpclient.c - A simple TCP client
 * usage: tcpclient <host> <port>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <fcntl.h>

#define BUFSIZE 1024

/* 
 * error - wrapper for perror
 */
void error(char *msg) {
    perror(msg);
    exit(0);
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

int sendf(int sockfd,char *file,int size) {
	int fd = open(file,O_RDONLY);
	int n=1,len,chk = BUFSIZE;	//chunk of BUFSIZE bytes
	char buff[BUFSIZE],ch[2];
	while(size > 0) {
		if(size < chk) len = size;
		else len = chk; 
		size -= chk;
		read(fd,buff,len);		//reading len bytes
		n = write(sockfd,buff,len);	//sending them to the server
	}
	close(fd);
	if(n) return 1;
	else return 0;
}


int main(int argc, char **argv) {
    int sockfd, portno, n;
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
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");

    /* gethostbyname: get the server's DNS entry */
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

    /* connect: create a connection with the server */
    if (connect(sockfd, &serveraddr, sizeof(serveraddr)) < 0) 
      error("ERROR connecting");

    //Entering the details of the file	
	int filesize = 0;    
	char file[400],*hash;
    printf("Enter the name of the file - ");
	scanf("%s",file);
	filesize = fsize(file);
	hash = hashsum(file);
	
    /* get message line from the user */
    bzero(buf, BUFSIZE);
 	sprintf(buf,"Hello %s %d",file,filesize);

    /* send the message line to the server */
    n = write(sockfd, buf, strlen(buf));
    if (n < 0) 
      error("ERROR writing to socket");
	printf("Sent : %s\n",buf);

    /* print the server's reply */
    bzero(buf, BUFSIZE);
    n = read(sockfd, buf, BUFSIZE);
    if (n < 0) 
      error("ERROR reading from socket");
    printf("Echo from server: %s", buf);
	
	if(!sendf(sockfd,file,filesize)) {
		error("ERROR sending the file");
	}
	
	/* check the server's reply */
    bzero(buf, BUFSIZE);
    n = read(sockfd, buf, BUFSIZE);
    if (n < 0) 
      error("ERROR reading from socket");
    printf("\nEcho from server: %s\nHash sent is %s\n", buf, hash);

	if(strcmp(buf,hash)) printf("MD5 not matched\n");
	else printf("MD5 matched\n");

    close(sockfd);
    return 0;
}
