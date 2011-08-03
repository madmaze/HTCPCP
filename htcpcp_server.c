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
#include <time.h>
#include "pot.c"


#define QUEUELENGTH	10
#define RCVBUFSIZE	256
#define METHOD		0
#define VAR		1
#define TRUE 		1
#define FALSE		0

#define POTCNT 5

typedef struct {
	pthread_t tid;
	int T_id;
	int sock;
	int busy;
	potStruct *pot;
} T_vars;

T_vars threadVars[POTCNT+1];

potStruct Pots[POTCNT];

////////////////////////////////////////////////////////////////////
// splits requests, either <var>:<val> or <method> <pot>
////////////////////////////////////////////////////////////////////
int splitVarVal(char* buf, char* var, char* val, char tdel){
	const char *del;
	int varLen;
	char* tmp;
	if(tdel==';')
		del=";";
	else
		del=":";
	
	if( strstr( buf, del )!=NULL ){
		varLen = (strlen(buf)-strlen(strstr(buf, del ))); 	    
		strncpy(var,buf,varLen);
		var[varLen]='\0';
		//printf( "var: |%s|\n", var );
		strcpy(val,buf+varLen+1); // +1 for skipping ":"
		//printf( "val: |%s|\n", val );
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

void strip(char* str, int bufLen){
	// do nothing
	printf("stripping...\n");
}

void CoffeeRequestHandler( char *buff, int *potNum, char *method, char additions[1024]) {
	char *line = NULL;
	char lineBuf[1024];
	char varBuf[255];
	char valBuf[255];
	char tmpBuf[255];
	char *var = NULL;
	int varlen=0;
	int cnt=0;
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
	    type = splitVarVal(lineBuf,varBuf,valBuf,':');
	    if (type == METHOD){
	    	    printf("Its a method. %s=%s\n",varBuf,valBuf);
	    	    strip(varBuf,255);
	    	    strcpy(method,varBuf);
	    	    *potNum=atoi(strstr(valBuf,"-")+1);
	    } else if ( type == VAR ) {
	    	    memset(tmpBuf, 0, 255);
	    	    printf("Its a var. %s=%s\n",varBuf,valBuf);
	    	    if(strcmp(varBuf, "Accept-Additions")==0){
	    	    	    strip(varBuf,255);
	    	    	    strcpy(additions,valBuf);
	    	    }
	    }
	    line = strtok( NULL, LineDel );
	}
}

static void *thread(void *ptr) {
	T_vars *vars;
	vars = (T_vars *) ptr;
	char method[255];
	int potNum;
	int type;
	char *line=NULL;
	char tmpAdd[1024];
	char lineBuf[1024];
	char varBuf[255];
	char valBuf[255];
	const char del[] = ",";
	potNum=1;
	
	printf("Thread %d\n",(int)vars->T_id);
	fflush(stdout);

	char buff[1024];
	int addCnt=0;
	int bytesRcvd,x;

	memset(buff, 0, 1024);
	memset(method, 0, 255);

	// Read client request
	if ((bytesRcvd = read((int)vars->sock, buff, RCVBUFSIZE - 1)) <= 0) {
		perror("read");
		exit(-1);
	}

	CoffeeRequestHandler(buff, &potNum, method, tmpAdd);
	
	line = strtok( tmpAdd , del);
	while( line != NULL ) {
		 printf( "extracted req: |%s|\n", line );
		 strip(line,255);
		 strcpy(lineBuf,line);
		 type = splitVarVal(lineBuf,varBuf,valBuf,';');
		 sprintf((vars->pot[potNum]).waitingAdditions[addCnt],"%s;%s",varBuf,valBuf);
		 //strcpy((vars->pot[potNum]).waitingAdditions[addCnt],"test");
		 line = strtok( NULL, del );
		 addCnt++;
	}
	for(x=0;x<addCnt;x++){
		printf("add %s\n",(vars->pot[potNum]).waitingAdditions[x]);
	}
	printf("METHOD is |%s|\n",method);
	printf("POT num is |%d|\n",potNum);
	
	// LOGIC HERE
	
	// stop and close socket
	shutdown((int)vars->sock, 1);
	close((int)vars->sock);
	vars->busy = FALSE;	
}

////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////
int main(int argc, char **argv, char **environ) {
  	pid_t pid;
  	char tmpBuf[RCVBUFSIZE];
	int sockid, tmpsock;
	int PORT;
	int i;
	int curThread;

	// structs to hold addrs
  	struct sockaddr_in server_addr, client_addr; 

	char coffee_request[512];	// coffe request
	
	for(i=0; i<POTCNT; i++){
		resetPot(&Pots[i]);
		Pots[i].cupWaiting=i*10;
	}

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
	int client_len = sizeof(client_addr);		
	int clientaddrlength;
	clientaddrlength = sizeof(client_addr);


	// while not killed accept sockets
  	while (1) {
  		curThread=0;
  		while(threadVars[curThread].busy == TRUE && curThread < POTCNT){
  			curThread++;
  		}
  		
  		if(curThread == POTCNT){
  			printf("TOO MANY CONNECTIONS\n");
  			tmpsock = accept(sockid, (struct sockaddr *) &client_addr, &clientaddrlength);
  			strcpy(tmpBuf,"HTCPCP/1.0 418 I'm a teapot\r\n");
  			if (write(tmpsock, tmpBuf, strlen((char*)tmpBuf)) <= 0) {
  				perror("write");
  				exit(-1);
			}	
  		} else {
			
			threadVars[curThread].sock = accept(sockid, (struct sockaddr *) &client_addr, &clientaddrlength);
			threadVars[curThread].pot = Pots;
			if (threadVars[curThread].sock < 0) {
				perror("accept");
				exit(-1);
			}
			
			threadVars[curThread].T_id = curThread;
			threadVars[curThread].busy = TRUE;
			if( pthread_create(&threadVars[curThread].tid, NULL, &thread, (void *) &threadVars[curThread]) !=0 ) {
				perror("pthread_create");
				exit(-2);
			}
			printf("Created thread %d\n",curThread);
		}
		//curThread++;
/*
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
    		*/
		// parent, dont care about this socket
		//close(newsock);	
  	}
}
