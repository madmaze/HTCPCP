/////////////////////////////////////////////////////////////////////////
//
// HTCPCP implementation
// Hyper Text Coffee Pot Control Protocol
//
// Copyleft/Copyright (c)2011
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

char* strip(char* str){
	char *end;
	
	// Trim leading space
	while(isspace(*str)) str++;
	
	if(*str == 0)  // All spaces?
	return str;
	
	// Trim trailing space
	end = str + strlen(str) - 1;
	while(end > str && isspace(*end)) end--;
	
	// Write new null terminator
	*(end+1) = 0;
	
	return str;
}

int CoffeeRequestHandler( char *buff, int *potNum, char *method, char additions[1024]) {
	char *line = NULL;
	char lineBuf[1024];
	char varBuf[255];
	char valBuf[255];
	char tmpBuf[255];
	int type;
	const char LineDel[] = "\r\n";
	
	printf("\nRequest:\n%s\n",buff);
	fflush(stdout);

	line = strtok( buff , LineDel);
	if(line == NULL)
		return -1;
	while( line != NULL ) {
	    //printf( "extracted req: |%s|\n", strip(line) );
	    strcpy(lineBuf,line);
	    type = splitVarVal(lineBuf,varBuf,valBuf,':');
	    if (type == METHOD){
	    	    //printf("Its a method. %s=%s\n",varBuf,valBuf);
	    	    strcpy(method,strip(varBuf));
	    	    *potNum=atoi(strstr(valBuf,"-")+1);
	    } else if ( type == VAR ) {
	    	    memset(tmpBuf, 0, 255);
	    	    //printf("Its a var. %s=%s\n",varBuf,valBuf);
	    	    if(strcmp(varBuf, "Accept-Additions")==0){
	    	    	    strcpy(additions,strip(valBuf));
	    	    }
	    }
	    line = strtok( NULL, LineDel );
	}
	if(strlen(varBuf)<1 || strlen(valBuf)<1)
		return -1;
	else
		return 0;
}

static void *thread(void *ptr) {
	T_vars *vars;
	vars = (T_vars *) ptr;
	char method[255];
	int potNum;
	int type;
	char *line=NULL;
	char tmpAdd[1024];
	char lineBuf[2048];
	char varBuf[255];
	char valBuf[255];
	const char del[] = ",";
	potNum=1;
	char Adds[20][255];
	
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

	if(CoffeeRequestHandler(buff, &potNum, method, tmpAdd)>=0){
	
	line = strtok( tmpAdd , del);
	while( line != NULL ) {
		 printf( "extracted add: |%s|\n", line );
		 //strip(line,255);
		 strcpy(lineBuf,strip(line));
		 type = splitVarVal(lineBuf,varBuf,valBuf,';');
		 sprintf(Adds[addCnt],"%s;%s",varBuf,valBuf);
		 //strcpy((vars->pot[potNum]).waitingAdditions[addCnt],"test");
		 line = strtok( NULL, del );
		 addCnt++;
	}
	for(x=0;x<addCnt;x++){
		printf("add %s\n",Adds[x]);
	}
	printf("METHOD is %s\n",method);
	printf("POT num is %d\n",potNum);
	} else {
		printf("Parsing error!\n");
	}
	// LOGIC HERE
	
	if( strcmp(method,"BREW") ==0 ){
		brew(&vars->pot[potNum], Adds, lineBuf);
	} else if( strcmp(method,"PUT") == 0) {
		put(&vars->pot[potNum], lineBuf);
	} else if( strcmp(method,"GET") == 0) {
		get(&vars->pot[potNum], lineBuf);
	} else if( strcmp(method,"WHEN") == 0) {
		when(&vars->pot[potNum], lineBuf);
	} /*else if( strcmp(method,"PROPFIND") == 0) {
		propfind(&vars->pot[potNum], lineBuf);
	}*/
	else{
		strcpy(lineBuf,"HTCPCP/1.0 418 I'm a teapot\r\n");
	}
	
	if (write((int)vars->sock, lineBuf, strlen((char*)lineBuf)) <= 0) {
		perror("write");
		exit(-1);
	}
	
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

	for(i=0; i<POTCNT; i++){
		resetPot(&Pots[i]);
		//Pots[i].cupWaiting=i*10;
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
	unsigned int clientaddrlength;
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
  	}
}
