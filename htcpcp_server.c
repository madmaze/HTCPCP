/////////////////////////////////////////////////////////////////////////
//
// HTCPCP implementation
// Hyper Text Coffee Pot Control Protocol
//
// Aug 1st 2011
// Matthias Lee
// James Eastwood
/////////////////////////////////////////////////////////////////////////
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>

#include <stdlib.h>
#include <sys/wait.h>
#include <netinet/in.h>

#include <signal.h>


#define QUEUELENGTH	10
#define RCVBUFSIZE	256
#define METHOD		0
#define VAR		1

struct mType
    {
        char ext[255];
        char mimeType[255];
    } ;


////////////////////////////////////////////////////////////////////
// splits requests, either <var>:<val> or <method> <pot>
////////////////////////////////////////////////////////////////////
int splitVarVal(char* buf, char* var, char* val){
	int varLen;
	char* tmp;
	if( strstr( buf, ":" )!=NULL ){
		varLen = (strlen(buf)-strlen(strstr(buf, ":" ))); 	    
		strncpy(var,buf,varLen);
		var[varLen]='\0';
		strcpy(val,buf+varLen+1); // +1 for skipping ":"
		//printf( "var: |%s|\n", var );
		return VAR;
	} else {
		varLen = (strlen(buf)-strlen(strstr(buf, " " ))); 	    
		strncpy(var,buf,varLen);
		var[varLen]='\0';
		tmp=buf+varLen+1; // +1 for skipping " "
		varLen = (strlen(tmp)-strlen(strstr(tmp, " " )));
		strncpy(val,tmp,varLen);
		//printf( "Method/pot: |%s|%s|\n", var, val );
		return METHOD;
	}
}

void CoffeeRequestHandler( char *buff, int newsock) {
	char *line = NULL;
	char lineBuf[1024];
	char varBuf[256];
	char valBuf[256];
	char *var = NULL;
	int varlen=0;
	int type;
	const char LineDel[] = "\r\n";
	const char VarDel[] = ": ";
	const char SpaceDel[] = " ";
	
	printf("\nRequest:\n%s\n",buff);
	fflush(stdout);

	line = strtok( buff , LineDel);
	while( line != NULL ) {
	    printf( "extracted req: |%s|\n", line );
	    strcpy(lineBuf,line);
	    type = splitVarVal(lineBuf,varBuf,valBuf);
	    if (type == METHOD){
	    	    printf("Its a method. %s=%s\n",varBuf,valBuf);
	    } else if ( type == VAR ) {
	    	    printf("Its a var. %s=%s\n",varBuf,valBuf);
	    }
	    line = strtok( NULL, LineDel );
	}
	
}

////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////
int main(int argc, char **argv, char **environ) {
  	pid_t pid;
	int sockid;
	int PORT;

	// structs to hold addrs
  	struct sockaddr_in server_addr, client_addr; 

	char coffee_request[512];	// coffe request

        struct mType mimeTypes[700];   

	if( argc != 2 ) {
		fprintf(stderr, "USAGE: %s <port number>\n", argv[0]);
		exit(-1);
	}

	PORT = atoi(argv[1]);		// parse port num


	if ( (pid = fork()) < 0) {
		perror("Cannot fork");
		exit(0);
  	}
	else if (pid != 0) {
		// keep track of pids for final kill
		char t[128];
		sprintf(t, "echo %d > %s.pid\n", pid, argv[0]);
		system(t);
    		exit(0);
  	}

	// Create, bind it, listen on socket
	signal(SIGCHLD, SIG_IGN);
  	if( (sockid = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    		perror("socket");
		exit(-1);
	}
	
	// setup sockaddr
  	bzero(&server_addr, sizeof(server_addr));
  	server_addr.sin_family = AF_INET;

        // accept any connection on PORT 
  	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  	server_addr.sin_port = htons(PORT);

	// bind
  	if (bind(sockid, (struct sockaddr *) &server_addr, sizeof(server_addr)) != 0) {
		perror("can't bind to socket");
		exit(-1);
	}

	if (listen(sockid, QUEUELENGTH) < 0) {
    		perror("listen");
		exit(-1);
	}


	// while not killed accept sockets
  	while (1) {

		int newsock;		
		int client_len = sizeof(client_addr);
		
		
		int clientaddrlength;
		clientaddrlength = sizeof(client_addr);
		newsock = accept(sockid, (struct server_addr *) &client_addr, &clientaddrlength);

    		if (newsock < 0) {
			perror("accept");
			exit(-1);
		}

		if ( (pid = fork()) < 0) {
			perror("Cannot fork");
			exit(0);
  		}
		else if( pid == 0 ) {
			// spawn child process
			int r;
      			char buff[1024];
			int read_so_far = 0;
			int bytesRcvd;
			char ref[1024], rowRef[1024];

			close(sockid);

			memset(buff, 0, 1024);

			// Read client request
			if ((bytesRcvd = read(newsock, buff, RCVBUFSIZE - 1)) <= 0) {
				perror("read");
				exit(-1);
			}
			
			CoffeeRequestHandler(buff, newsock);

			
			// stop and close socket
			shutdown(newsock, 1);
			close(newsock);
			printf("\n");
			exit(0);
    		}
		// parent, dont care about this socket
		close(newsock);	
  	}
}
