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
#include <signal.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <fcntl.h>
#include <iostream>
#include <queue>

#define BUFSIZE 1074	//1024 + 50
#define HASHSIZE 32
#define TIMEOUT 1
#define MAXSEQ 1000000	//The maximum sequence no
#define MAX_WINDOW 10000

using namespace std;

int chunk = 1024;	//size of file chunks
int N;
int alarm_fired = 0;
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

void mysig(int sig)
{
    //printf("PARENT : Received signal %d \n", sig);
    if (sig == SIGALRM)
    {
        alarm_fired = 1;
    }
    signal(SIGALRM,mysig);
}

int conv(char *buf) {
	int s = 0,k = 1;
	for(int i=3;i>=0;i--) {
		int x = (int) buf[i];
		if(buf[i] == 'x')
			x = 0;
		s += k*x;
		k *= 256;
	}
	return s;
}


int sendf(struct sockaddr* serveraddr,int serverlen,int sockfd,char *file,int size) {
	int fd = open(file,O_RDONLY);
	char seq[4];
	int seqn = 0;
	char buff[BUFSIZE],buf[BUFSIZE],ack[5];
	int comm[2];
	pipe(comm);	//pipe for implementing a time out
	socklen_t len;
	int basep,currp = -1;
	queue< string > Q,P;
	while(size || !Q.empty()) {
		basep = currp + N;
		int i = currp + 1;
		while(i <= basep) {
			//***cout << "i = " << i << " , basep = " << basep << "\n";
			if(Q.empty()) {
				int chk = chunk;
				if(size < chunk) chk = size;
				if(size <= 0) break;
				size -= chk;
				bzero(buff,BUFSIZE);

				int d[4];
				int m = chk,p = seqn;
				for(int j=0;j<4;j++) {
					d[j] = m%10;
					seq[j] = (p%256 == 0) ? 'x' : p%256;
					m = m/10;
					p = p/256;
				}
				bzero(buff,BUFSIZE);
				sprintf(buff,"%c%c%c%c%d%d%d%d",seq[3],seq[2],seq[1],seq[0],d[3],d[2],d[1],d[0]);	//Preparing packet header
				bzero(buf,BUFSIZE);

				int n = read(fd,buf,chk);
				if(n!=chk) error("Not able to read characters");
				strcat(buff,buf);	//Packet is Ready in buff
				P.emplace(buff);
				seqn = (seqn+1)%MAXSEQ;
			} else {
				string s = Q.front();
				bzero(buff,BUFSIZE);
				strcat(buff,s.c_str());
				P.push(Q.front());
				Q.pop();
			}
			//sending the packet to the destination
			sendto(sockfd,buff,strlen(buff),0,serveraddr,serverlen);
			i++;

		}
		//after sending all N packets we then start a timer
		bzero(buf,BUFSIZE);
		len = sizeof(serveraddr);
		int fack = currp;	//last acked packet
		alarm_fired = 0;
		alarm(TIMEOUT);
		int seqbase,seqcurr;		//seq no corresponding to last sent packet and first send packet in the window
		seqbase = conv(const_cast<char*>((P.back()).c_str()));
		seqcurr = conv(const_cast<char*>((P.front()).c_str()));
		while(alarm_fired==0) {
			bzero(buf,BUFSIZE);
			int n = recvfrom(sockfd, buf,BUFSIZE,0,(struct sockaddr *) serveraddr, &len);
			if(n) {
				printf("ACK is %s\n",buf);
				fack = atoi(buf+3);
				//cout << "fack - " << fack << "\n";
				if(fack <= seqbase || fack >= seqcurr) {
					alarm(0);		//cancelling the present alarm
					break;
				}
			}
		}	
		int ackcount = 0;			//no of packets acknowledged
		while(fack != currp && !P.empty()) {	//if we have received atleast one acknowledgement then only remove them from the queue
			if(conv(const_cast<char*>((P.front()).c_str()))==fack) {
				P.pop();
				ackcount++;
				break;
			}
			P.pop();	//removing the elements which are acknowledged from the window
			ackcount++;
		}
		while(!P.empty()) {
			Q.push(P.front());
			P.pop();
		}
		//***cout << "currp - " << currp << " , fack - " << fack << " , basep - " << basep << "\n";
		currp = fack;
		if(ackcount==0) N = N/2 + 1;			//Half the windowsize if no packet ack is received
		else N = N + ackcount;					//Increase the window size by no of packets acknowledged
		if(N >= MAX_WINDOW) N = MAX_WINDOW - 1;
	}

	/*while(size>0) {
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
		
		sprintf(buff,"%d %d%d%d%d",seq,d[3],d[2],d[1],d[0]);	//Preparing packet header
		bzero(buf,BUFSIZE);

		int n = read(fd,buf,chk);
		if(n!=chk) error("Not able to read characters");
		strcat(buff,buf);	//Packet is Ready in buff

		int flag = 0;//ACK packet not received
		socklen_t len;
	

		while(!flag) {
			sendto(sockfd,buff,chk+8,0,serveraddr,serverlen);
			//temp
			//sleep(1);
			len = sizeof(serveraddr);
			int n = recvfrom(sockfd, buf, 4,0,(struct sockaddr *) serveraddr, &len);
			if(n!=4) continue;
			buf[4] = '\0';
			printf("ACK has been received - %s\n",buf);
			sprintf(ack,"ACK%d",seq);
			if(strcmp(ack,buf)==0) flag = 1;
			//temp
			//Timeout implementation
			//recvfrom(buf,4);if(strcmp(buf,"ACK<seq>")==0) flag = 1;
		}
		seq = (seq+1)%(N+1);	
	} */
	close(fd);
	alarm(0);			//canceling the present alarm if any
	return 1;
}

int main(int argc, char **argv) {
    int sockfd, portno, n;
    socklen_t serverlen;
    struct sockaddr_in serveraddr;
    struct hostent *server;
    char *hostname;
    char buf[BUFSIZE];
    signal(SIGALRM,mysig);
    /* check command line arguments */
    if (argc != 3) {
       fprintf(stderr,"usage: %s <hostname> <port>\n", argv[0]);
       exit(0);
    }
    hostname = argv[1];
    portno = atoi(argv[2]);
    //N = atoi(argv[3]);
	N = 3;	//Setting the starting window size = 3
    /* socket: create the socket */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");

    /* gethostbyname: get the server's DNS entry */
	
	struct timeval tv;
	tv.tv_sec = 1;	//one second timeout
	tv.tv_usec = 0;	
	/*if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv)) < 0) {
		error("Setting the timeout");
	}*/
	
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
    int  flag = 20;			//maximum no of times to try for getting the hash
	while(flag) {
		bzero(buf,BUFSIZE);
		n = recvfrom(sockfd, buf, HASHSIZE, 0, (struct sockaddr *) &serveraddr, &serverlen);
		if (n < 0) 
		  error("ERROR in recvfrom in receiving the hash");
		if(n==32) break;
		flag--;
	}
	if(flag==0) bzero(buf,BUFSIZE);
    printf("Echo from server: %s\n", buf);	
	printf("Original Hash is %s\n",hash);
	/*checking the received hash*/	
	flag = 0;
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





