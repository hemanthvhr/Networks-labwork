/*
 * p2pchatserver.cpp , a peer to peer chat server
 * usage: p2pchat <username>
 */

#include <iostream>
#include <string>
#include <unordered_map>
#include <utility>
#include <set>
#include <vector>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <string.h>

#define BUFSIZE 1024

using namespace std;

//unordered_map < string , int > address;
//vector < string > contact;

unordered_map< string , pair < string , int > > contacts;
unordered_map< string , int > nfds; //correspondence between users and the connection file descriptors


int maxi(int x,int y) {
	if(x > y) return x;
	return y;
}

void error(char *msg) {
  perror(msg);
  exit(1);
}

int search(vector<int> &A,int a) {
    for(int i=0;i<A.size();i++) {
        if(A[i]==a) return i;
    }
    return -1;
}

/*void contacts() {
	int i = 0;
	string row;
	char input[100],*info[3];
	bzero(input,100);
	getline(cin,row);
	cin >> ws;
	cout << row;
	while(!row.empty()) {
		printf("ok%d\n",i+1);
		strcpy(input,row.c_str());
		char *token = strtok(input," ");
		int j = 0;
		printf("fine\n");
		while(token!=NULL) {
			printf("%d : %s\n",j+1,token);
			info[j++] = token;
			strtok(NULL," ");
		}
		address.emplace(info[0],i);
		address.emplace(info[1],i);
		contact.push_back(row);
		i++;
		getline(cin,row);
		cin >> ws;
	}
}
*/

int main(int argc,char** argv) {

	/*initializing contacts list*/
	contacts[string("pcone")] = make_pair("localhost",2345);
	contacts[string("pctwo")] = make_pair("localhost",2675);
	contacts[string("pcthree")] = make_pair("localhost",4567);
	contacts[string("pcfour")] = make_pair("localhost",6788);
	contacts[string("pcfive")] = make_pair("localhost",5434);
	contacts[string("pcsix")] = make_pair("localhost",4598);
	/*----------------------------------------------------------------------------------------*/
	
	int maxfd = 0;	//max file descriptor no
	int sockfd; /* socket file descriptor - an ID to uniquely identify a socket by the application program */
	int portno; /* port to listen on */
	socklen_t clientlen; /* byte size of client's address */
	struct sockaddr_in serveraddr; /* server's addr */
	struct sockaddr_in clientaddr; /* client addr */
	struct hostent *hostp; /* client host info */
	char *hostaddrp; /* dotted decimal host addr string */
	int optval; /* flag value for setsockopt */
	char *myname;	//name of the present user
	char buff[BUFSIZE],msg[BUFSIZE];

	  /*
	* check command line arguments
	*/
	if (argc != 2) {
		fprintf(stderr, "usage: %s <user_name>\n", argv[0]);
		exit(1);
	}
	myname = new char[strlen(argv[1])+1];
	strcpy(myname,argv[1]);
	portno = contacts[string(myname)].second;
	cout << "Ok " << portno << "\n";

	/*
	* socket: create the socket
	*/
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
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
	int on = 1;
	if(ioctl(sockfd, FIONBIO, (char *)&on) < 0) 
		error("ERROR at ioctl()");
	
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
	if (bind(sockfd, (struct sockaddr *) &serveraddr,sizeof(serveraddr)) < 0)
		error("ERROR on binding");
	if (listen(sockfd, 5) < 0) /* allow 5 requests to queue up */ 
    	error("ERROR on listen");
	printf("Server Running ....\n");

	vector< int > cfds;	//for storing all other currently connected clients connection file descriptors
	fd_set rfds,efds;
	bool outgoing = false;  //if we have connected to any other connection initiated by us
	struct timeval tv;	//timeout for the select call
	tv.tv_sec = 1;	//1 second
	tv.tv_usec = 0;
	while(true) {
		maxfd = 0;
		FD_ZERO(&rfds);
		FD_ZERO(&efds);
		FD_SET(0,&rfds);	//adding stdin to rfds
		FD_SET(sockfd,&rfds);maxfd = maxi(sockfd,maxfd);
		for(int i=0;i<cfds.size();i++) {
			FD_SET(cfds[i],&rfds);
			if(maxfd < cfds[i]) maxfd = cfds[i];
		}
		int res = select(maxfd+1,&rfds,nullptr,nullptr,nullptr);	//for now no timeout
		if(res == -1) error("Error in select call");
		/* Check which file descriptor returns the select */
		if(FD_ISSET(0,&rfds) && res) {			//checking for the stdin
			/*input the message and send it to the server*/
            scanf("%s",&msg);       //message should be of the form "<user>/<text>"
            char *usr = strtok(msg,"/");
            char *text = strtok(NULL,"/");
            string name(usr);
			if(contacts.find(name) == contacts.end()) {
                printf("There is no such user %s\n",usr);
			} else {
		        if(nfds.find(name) != nfds.end()) {  //if there is a such user
		            int x = search(cfds,nfds[name]);  //if a connection with the user is already established
		            if(x != -1) {
		                if(write(cfds[x],text,strlen(text)) <= 0) {
		                    perror("ERROR while sending message to the user");
		                }
		            } else {
		                perror("ERROR a bug in the code");
		            }
		            printf("Message is sent to %s : %s\n",usr,text);
		        } else {
					char *hostname = const_cast<char*>((contacts[name].first).c_str());
		            int port = contacts[name].second;
		            /* gethostbyname: get the server's DNS entry */
		            hostp = gethostbyname(hostname);
		            if (hostp == NULL) {
		                fprintf(stderr,"ERROR, no such host as %s\n", hostname);
		                exit(0);
		            }

		            /* build the server's Internet address */
		            bzero((char *) &clientaddr, sizeof(clientaddr));
		            clientaddr.sin_family = AF_INET;
		            bcopy((char *)hostp->h_addr,
		              (char *)&clientaddr.sin_addr.s_addr, hostp->h_length);
		            clientaddr.sin_port = htons(port);
					printf("addr is %s : %d\n",hostname,port);

		            /* connect: create a connection with the user */
		            if(outgoing) {
		                for(auto it = nfds.begin();it!=nfds.end();it++) {     //removing previous associations with that sockfd
		                    if(it->second == sockfd) {
		                        nfds.erase(it);
		                        break;
		                    }
		                }
		                close(sockfd);
		            } else {
		                cfds.push_back(sockfd);
		                outgoing = true;
		            }
		            if (connect(sockfd, (const sockaddr*)&clientaddr, sizeof(clientaddr)) < 0)
		            	error("ERROR connecting");
		            nfds[name] = sockfd;
		            if(write(sockfd,text,strlen(text)) <= 0) {
		                perror("ERROR while sending message to the user");
		            }
		        }
			}
			res--;
		}
		if(FD_ISSET(sockfd,&rfds) && res) {
			/*accept the incoming connecton from another server*/
			while(true) {
				clientlen = sizeof(clientaddr);
				int childfd = accept(sockfd, (struct sockaddr *) &clientaddr, &clientlen);
				if(childfd < 0) break;
				cfds.push_back(childfd);
				if (childfd < 0)
				  error("ERROR on accept");
				hostp = gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr,
					  sizeof(clientaddr.sin_addr.s_addr), AF_INET);
				if (hostp == NULL)
				  error("ERROR on gethostbyaddr");
				hostaddrp = inet_ntoa(clientaddr.sin_addr);
				if (hostaddrp == NULL)
				  error("ERROR on inet_ntoa\n");
				printf("server established connection with %s (%s)\n",
				   hostp->h_name, hostaddrp);
		        pair< string , int > p = make_pair((char *)&clientaddr.sin_addr.s_addr,ntohs(clientaddr.sin_port));
		        string client = string("unknown");
		        for(auto it = contacts.begin();it!=contacts.end();it++) {
		            if(it->second == p) {
		                client = it->first;
		                break;
		            }
		        }
		        nfds[client] = childfd;
		        bzero(buff,BUFSIZE);
		        if(read(childfd,buff,BUFSIZE) < 0) {
		            perror("ERROR at reading the accepted connections message");
		        }
		        printf("%s/%s\n",client.c_str(),buff);      //print the message to the console
				/*read the message*/
				FD_CLR(sockfd,&rfds);
			}
			res--;
		}
		for(int i=0;i<cfds.size();i++) {
            if(FD_ISSET(cfds[i],&rfds) && res) {
                /*read the message*/
                string client;
                for(auto it = nfds.begin();it!=nfds.end();) {
                    if(it->second == cfds[i]) {
                        client = it->first;
                        break;
                    }
                    it++;
                }
                bzero(buff,BUFSIZE);
                if(read(cfds[i],buff,BUFSIZE) < 0) {
                    perror("ERROR at reading the message of already connected user");
                }
                printf("%s/%s\n",client.c_str(),buff);      //display the user message
                res--;
            }
		}

	}
    return 0;
}
























