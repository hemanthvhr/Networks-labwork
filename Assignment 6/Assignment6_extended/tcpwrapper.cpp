#include "tcpwrapper.h"
/*http://www.drdobbs.com/creating-stl-containers-in-shared-memory/184401639*/

//packet structure - Data -- 1|seq|size|data
//					 ACK  -- 0|seq|rwndsize

//implement areceiverside buffer and in receiver call if a data packet is received an acknowledgement is sent ,and the same sockfd resource is protected using a mutex or semaphore
#include <queue>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <signal.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <fcntl.h>
#include <iostream>
#include <pthread.h>
#include <semaphore.h>

using namespace std;

sem_t lock,lock2;
int alarm_fired = 0;
int baseptr;
int cwnd = 3,rwnd = cwnd,w;
int recvack,resend;
int recset=0,recrwd,recseq;	//values set by the reveiver thread 
int ssthreash = SST;			//slow start threshold
int prevcontrol = 1;	//if there is no packet to be sent in the sender buffer
queue<char> S;	//sender buffer
queue<char> R;	//receiver buffer



int sockfd;
struct sockaddr *destaddr,destination;
socklen_t destlen;


void error(char *msg) {
	perror(msg);
	exit(0);
}

int min(int x,int y) {
	if(x < y) return x;
	else return y;
}

void bytenum(char *buf,int x) {
	int i = 4;
	while(i--) {
		buf[i-1] = x%256;
		if(buf[i-1] == '\0') buf[i-1] = 'x';
		x /= 256;
	} 
}

int conv(char *buf) {
	int sum = 0,p = 1;
	for(int i=3;i>=0;i--) {
		int a = buf[i];
		if(a == 'x') a = 0;
		sum += p*a;
		p *= 256;
	}
	return sum;
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


int bufferHandle(char *buff,int size) {	//Handling the sender buffer
	int i=0;
	while(size) {
		while(i==0) {
			sem_wait(&lock);
			i = SBUFF - S.size();
			if(i==0) sem_post(&lock);
		}
		if(i > size) i = size;
		size -= i;
		for(int j=0;j<i;j++) {
			S.push(buff[j]);
		}
		sem_post(&lock);
		buff += i;
	}	
	return size;
}

void send_ack(int next_seq,int rwd) {
	char buf[BUFSIZE];
	bzero(buf,BUFSIZE);
	//preparing the ack packet
	sprintf(buf,"0");
	bytenum(buf+1,next_seq);
	bytenum(buf+5,rwd);
	sendto(sockfd,buf,strlen(buf),0,destaddr,destlen);
}

void update_window(int x) {
	if(x <= baseptr || x > (baseptr+w)) return;
	if(cwnd < ssthreash) {
			cwnd += x-baseptr;
	} else {
		cwnd += (int)(MSS/cwnd);
	}
}

void *rate_control(void *A) {
	int outstand = 0;
	int seq = 0;
	queue<char> P,Q;
	char buf[BUFSIZE];
	while(true) {	//appsend writes data into the sender buffer and as soon as there is some data in the buffer the rate_control thread just sends all the data
		printf("rate_control started\n");
		w = min(cwnd,rwnd);		//updatewindow function updates cwnd
		outstand = P.size();
		while(P.empty()) {
			sem_wait(&lock);
			while(P.size() <= w && !S.empty()) {
				P.push(S.front());
				S.pop();
			}
			sem_post(&lock);
			if(P.empty()==false) prevcontrol = 0;
			else prevcontrol = 1;
		}
		if(resend==1) outstand = 0;	//if timer for first byte goes off without any acknowledgement then send all the packets
		for(int i =0;i<outstand;i++) {
			Q.push(P.front());
			P.pop();
		}
		recvack = seq,resend = 0,recset = 0;
		baseptr = seq;
		//here recvack acts as the baseptr in a window and seq
		seq += outstand;
		int i = 0;
		while(!P.empty()) {	//handle the case where while sending the packets the ack for the first packet has been received
			int s = MSS;
			if(P.size() < s) s = P.size();
			bzero(buf,BUFSIZE);
			char seqe[4],len[4];
			bytenum(seqe,seq);
			bytenum(len,s);
			/***/sprintf(buf,"1%s%s",seqe,len);
			seq += s;
			for(int k=0;k<s;k++) {
				char c = P.front(),temp[2];
				Q.push(c);
				buf[9+k] = c;
				cout << buf[9+k] << "\n";
				P.pop();
			}
			printf("packet - %s\n",buf);
			/***/sendto(sockfd,buf,strlen(buf),0,destaddr,destlen);
			if(i==0) {
				alarm_fired = 0;				
				alarm(TIMEOUT);
			}
			i++;
		}
		while(alarm_fired==0 && recset==0);		//wait for ack receiver thread to wake this thread
		alarm(0);	//reset the alarm
		if(alarm_fired && recset == 0) {	//timeout event	
			resend = 1;
			cwnd = MSS;
		} else {
			if(recseq <= seq && recseq >= baseptr) {
				recvack = recseq;
				rwnd = recrwd;
			}
		}
		//
		//
		while(Q.size() != (seq - recvack)) Q.pop();
		seq = recvack;
	}
	return NULL;
}

void validate(char *buf,int *seq) {	//checking whether the received packet is of the form 'ACK____RWD___'
	if(buf[0]=='1') {	//it is a data packet
		int size,seqe,rwd;
		seqe = conv(buf+1);
		size = conv(buf+5);
		sem_wait(&lock2);
		for(int i=0;i<size;i++) {
			if(seqe == *seq) {	//only those packets whose seqnum is == the next expected seqnum
				R.push(buf[9+i]);	//write data into the receiver buffer
				*seq = *seq + 1;
			}
			seqe++;
		}
		rwd = SBUFF	- R.size();
		sem_post(&lock2);
		/*pthread_t temp;
		if(pthread_create(&temp,NULL,send_ack,NULL) < 0) {
			printf("Thread temp cannot be created\n");
		}*/
		send_ack(*seq,rwd);	//sending the Acknowledgement packet
	} else {			//it is a ack packet
		int rwd,seqe;
		seqe = conv(buf+1);
		rwd = conv(buf+5);
		update_window(seqe);	//update the window based on congestion control algorithms
		recrwd = rwd;
		recseq = seqe;
		recset = 1;
		printf("Received ack - %d\n",seqe);
	}
}

void *receiver(void *A) {
	int seq = 0;	//next expected sequence number
	char buf[BUFSIZE];
	cout << "receiver_started\n";
	struct sockaddr* recvaddr;
	socklen_t *len;
	while(true) {
		bzero(buf,BUFSIZE);
		int n = recvfrom(sockfd,buf,BUFSIZE,0,recvaddr,len);
		if(n > 0) validate(buf,&seq);
	}
	return NULL;
}

int appsend(char *buff,int size) {

	/*if(((struct sockaddr_in *)serveraddr)->sin_addr.s_addr != ((struct sockaddr_in *)destaddr)->sin_addr.s_addr || (((struct sockaddr_in *)serveraddr)->sin_port != ((struct sockaddr_in *)destaddr)->sin_port))
		while(prevcontrol == 0);	//if new address is different from the old one then wait untill old addr packets are gone
	destaddr = serveraddr;
	destlen = serverlen;
	destfd = sockfd;
	*/	//code to handle for multiple clients during same process
	bufferHandle(buff,size);
	return size;
}

int apprecv(char *buf,int bufsize) {
	int flag = 1,i = 0;
	while(flag) {
		sem_wait(&lock2);
		while(!R.empty() && i<bufsize) {
			flag = 0;
			buf[i] = R.front();
			R.pop();
			i++;
		}
		sem_post(&lock2);
	}
	return i+1;
}

int init_send(int port_no,char *destname,int dest_port) {
	signal(SIGALRM,mysig);
	sem_init(&lock,0,1);	//initializing the semaphore lock
	sem_init(&lock2,0,1);	//initializing the semaphore lock
	destaddr = &destination;
	struct sockaddr_in serveraddr;
    struct hostent *server;
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");
    int optval = 1;
  	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,(const void *)&optval , sizeof(int));
  	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 10000;	//10 ms timeout
	if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv)) < 0) {
		perror("Setting the timeout");
		return -1;
	}

	cout << "Ok3\n";
	/*
	* build the our server's Internet address
	*/
	bzero((char *) &serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons((unsigned short)port_no);

	/* 
	* bind: associate the parent socket with a port 
	*/
	if (bind(sockfd, (struct sockaddr *) &serveraddr, 
	   sizeof(serveraddr)) < 0) 
	error("ERROR on binding");
	
	cout << "Ok4\n";
	
	server = gethostbyname(destname);
	cout << "Ok5\n";
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host as %s\n", destname);
        exit(0);
    }
    cout << "mok1\n";

    /* build the server's Internet address */
    bzero((char *) destaddr, sizeof(destination));
    cout << "mok1\n";
    ((struct sockaddr_in *)destaddr)->sin_family = AF_INET;
    bcopy((char *)server->h_addr, (char *)&(((struct sockaddr_in *)destaddr)->sin_addr.s_addr), server->h_length);
    ((struct sockaddr_in *)destaddr)->sin_port = htons(dest_port);
	cout << "Ok6\n";

    destlen = sizeof(destination);

    pthread_t t1,t2;

    if(pthread_create(&t1,NULL,*rate_control,NULL) < 0) {
					printf("Thread 1 is not created\n");
	}
	if(pthread_create(&t2,NULL,*receiver,NULL) < 0) {
					printf("Thread 2 is not created\n");
	}
	return 1;
}