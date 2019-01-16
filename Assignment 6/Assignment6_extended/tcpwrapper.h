#include <semaphore.h>
#include <queue>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#include <sys/wait.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <fcntl.h>

#define TIMEOUT 1
#define SBUFF 1000000
#define MSS 1024
#define BUFSIZE 1040
#define SST 300

extern sem_t lock,lock2;
extern int alarm_fired;
extern int baseptr;
extern int cwnd,rwnd,w;
extern int recvack,resend;
extern int recset,recrwd,recseq;	//values set by the reveiver thread 
extern int ssthreash;			//slow start threshold
extern int prevcontrol;	//if there is no packet to be sent in the sender buffer
extern std::queue<char> S;	//sender buffer
extern std::queue<char> R;	//receiver buffer



extern int sockfd;
extern struct sockaddr *destaddr;
extern socklen_t destlen;


void error(char *msg);
int min(int x,int y);
void bytenum(char *buf,int x);
int conv(char *buf);
void mysig(int sig);
int bufferHandle(char *buff,int size);
void send_ack(int next_seq,int rwd);
void update_window(int x);
void *rate_control(void *A);
void validate(char *buf,int *seq);
void *receiver(void *A);
int appsend(char *buff,int size);
int apprecv(char *buf,int bufsize);
int init_send(int port_no,char *destname,int dest_port);