#include <stdio.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netdb.h>
#include <setjmp.h>
#include <errno.h>
#include <stdlib.h>
#include <time.h>
#define PACKET_SIZE     4096
#define MAX_WAIT_TIME   1   
int MAX_NO_PACKETS=3;


char sendpacket[PACKET_SIZE];
char recvpacket[PACKET_SIZE];
int sockfd, datalen = 56;
int nsend = 0, nreceived = 0;
struct sockaddr_in dest_addr;
pid_t pid;
struct sockaddr_in from;
struct timespec tvrecv;
void statistics(int signo,double* a);
unsigned short cal_chksum(unsigned short *addr, int len);
int pack(int pack_no);
void send_packet(void);
void recv_packet(double * a);
int unpack(char *buf, int len,double* a);
void tv_sub(struct timespec *out, struct timespec *in);

double sum(double *rtt) {
 int i;
 double tot=0;
 for(i=0;i<MAX_NO_PACKETS;i++)
      tot=tot+rtt[i];

 return tot;
}
void temp(double *rtt) {
  int i;
  double meand=0;
  double tot =rtt[0], min_ = rtt[0], max_ = rtt[0];

  for(i=1;i<MAX_NO_PACKETS;i++){
    min_ = (min_>rtt[i])?rtt[i]:min_;
    max_ = (max_<rtt[i])?rtt[i]:max_;
    tot  += rtt[i]; 
  } 
  double mean=tot/MAX_NO_PACKETS;
  for(i=0;i<MAX_NO_PACKETS;i++){
     meand += abs(rtt[i]-mean);
  } 
  printf(" rtt min/avg/max/mdev = %.3f/%.3f/%.3f/%.3f ms \n",min_,mean,max_,meand/MAX_NO_PACKETS);
}


void statistics(int signo,double * rtt) {

    printf("\n--------------------PING statistics-------------------\n");
    printf("%d packets transmitted, %d received , %%%d lost, total rtt = %.3f ms\n ", nsend,
    nreceived, (nsend - nreceived) / nsend * 100,sum(rtt));
    temp(rtt);
    close(sockfd);
    exit(1);
} 

unsigned short cal_chksum(unsigned short *addr, int len)
{
    int nleft = len;
    int sum = 0;
    unsigned short *w = addr;
    unsigned short answer = 0;
    while (nleft > 1)
    {
       sum +=  *w++;
        nleft -= 2;
    }
   if (nleft == 1)
    {
        char *x = (char *)w;
        //*(unsigned char*)(&answer) = *(unsigned char*)w;
        w = (unsigned short *)(--x);
        sum += (*w) & 0x00ff;
        //sum += answer;
    }
    sum = (sum >> 16) + (sum &0xffff);
    sum += (sum >> 16);
    answer = ~sum;
    return answer;
}
int pack(int pack_no)
{
    int i, packsize;
    struct icmp *icmp;
    struct timespec *tval;
    icmp = (struct icmp*)sendpacket;
    icmp->icmp_type = ICMP_ECHO;
    icmp->icmp_code = 0;
    icmp->icmp_cksum = 0;
    icmp->icmp_seq = pack_no;
    icmp->icmp_id = pid;
    packsize = 8+datalen;
    tval = (struct timespec*)icmp->icmp_data;
    clock_gettime(CLOCK_BOOTTIME, tval); 
    icmp->icmp_cksum = cal_chksum((unsigned short*)icmp, packsize); 
    return packsize;
}
void send_packet()
{
    int packetsize;
    while (nsend < MAX_NO_PACKETS)
    {
        nsend++;
        packetsize = pack(nsend); 
        if (sendto(sockfd, sendpacket, packetsize, 0, (struct sockaddr*)
            &dest_addr, sizeof(dest_addr)) < 0)
        {
            perror("sendto error");
            continue;
        } sleep(1); 
    }
}

void recv_packet(double * rtt)
{
    int n, fromlen;
    extern int errno;
    signal(SIGALRM, statistics);
    fromlen = sizeof(from);
    while (nreceived < nsend)
    {
        alarm(MAX_WAIT_TIME);
        if ((n = recvfrom(sockfd, recvpacket, sizeof(recvpacket), 0, (struct
            sockaddr*) &from, &fromlen)) < 0)
        {
            if (errno == EINTR)
                continue;
            perror("recvfrom error");
            continue;
        } clock_gettime(CLOCK_BOOTTIME,&tvrecv); 
        if (unpack(recvpacket, n,rtt) ==  - 1) {
            continue;
        }
        nreceived++;
    }
}
int unpack(char *buf, int len,double* RTT)
{
    int i, iphdrlen;
    struct ip *ip;
    struct icmp *icmp;
    struct timespec *tvsend;
    double rtt;
    ip = (struct ip*)buf;
    iphdrlen = ip->ip_hl << 2; 
    icmp = (struct icmp*)(buf + iphdrlen);
    len -= iphdrlen; 
    if (len < 24) {
        printf("ICMP packets\'s length is less than 24\n");
        return  - 1;
    } 
    if ((icmp->icmp_type == ICMP_ECHOREPLY) && (icmp->icmp_id == pid)) {
        tvsend = (struct timespec*)icmp->icmp_data;
        tv_sub(&tvrecv, tvsend); 
        rtt = tvrecv.tv_sec * 1000+tvrecv.tv_nsec / 1000000;
        printf("%d byte from %s: icmp_seq=%u ttl=%d rtt=%.3f ms\n", len,inet_ntoa(from.sin_addr), icmp->icmp_seq, ip->ip_ttl, rtt);
        RTT[(icmp->icmp_seq)-1]=rtt;
    }
    else
        return  - 1;
}
int main(int argc, char **argv)
{
    struct hostent *host;
    struct protoent *protocol;
    unsigned long inaddr = 0l;
    int waittime = MAX_WAIT_TIME;
    int size = 50 * 1024;
    if (argc < 2)
    {
        printf("usage:%s hostname/IP address\n", argv[0]);
        exit(1);
    } if ((protocol = getprotobyname("icmp")) == NULL)
    {
        perror("getprotobyname");
        exit(1);
    }
    if ((sockfd = socket(AF_INET, SOCK_RAW, protocol->p_proto)) < 0)
    {
        perror("socket error");
        exit(1);
    }
    if(argc==3)
       MAX_NO_PACKETS=atoi(argv[2]);
    setuid(getuid());
    setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size));
    bzero(&dest_addr, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    if (inaddr = inet_addr(argv[1]) == INADDR_NONE)
    {
        if ((host = gethostbyname(argv[1])) == NULL)
        {
           perror("gethostbyname error");
            exit(1);
        }
        memcpy((char*) &dest_addr.sin_addr, host->h_addr, host->h_length);
    }
    else
        dest_addr.sin_addr.s_addr = inet_addr(argv[1]);
    pid = getpid();
    printf("PING %s(%s): %d bytes data in ICMP packets.\n", argv[1], inet_ntoa(dest_addr.sin_addr), datalen);
    double *rtt;
    rtt=(double*)malloc(sizeof(double)*MAX_NO_PACKETS);
    send_packet(); 
    recv_packet(rtt); 
    statistics(SIGALRM,rtt); 
    return 0;
}
void tv_sub(struct timespec *out, struct timespec *in)
{
    if ((out->tv_nsec -= in->tv_nsec) < 0)
    {
        --out->tv_sec;
        out->tv_nsec += 1000000000;
    } out->tv_sec -= in->tv_sec;
}



