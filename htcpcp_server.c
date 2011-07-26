/////////////////////////////////////////////////////////////////////////
//
// To compile: 			gcc -o my_httpd my_httpd.c -lnsl -lsocket
//
// To start your server:	./my_httpd 2000 .www			
//
// To Kill your server:		kill_my_httpd
//
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

#include <dirent.h>
#include <signal.h>

#define	ERROR_FILE	0
#define REG_FILE  	1
#define DIRECTORY 	2
#define QUEUELENGTH	10
#define RCVBUFSIZE	256

struct mType
    {
        char ext[255];
        char mimeType[255];
    } ;

 
////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////
void GetMyHomeDir(char *myhome, char **environ) {
        int i=0;
	while( environ[i] != NULL ) {
        	if( ! strncmp(environ[i], "HOME=", 5) ){
			strcpy(myhome, &environ[i][5]);
			return;
		}
		i++;
        }
	fprintf(stderr, "[FATAL] SHOULD NEVER COME HERE\n");
	fflush(stderr);
	exit(-1);
}
////////////////////////////////////////////////////////////////////
// chops and returns extension of file
////////////////////////////////////////////////////////////////////
char* getExt(const char* str){
	char *last = strrchr(str, '.');
	printf("ext: %s\n",last);
	if(last==NULL)
		return("");
	else
		return(last);	
}

////////////////////////////////////////////////////////////////////
// read mime DB-file and parse/split into mimeTypes[]
////////////////////////////////////////////////////////////////////
void readMimeFile(struct mType *mimeTypes){
	FILE *mimes = fopen("mime.types", "rt");
	char line[100];
	char *result = NULL;
	const char dels[] = " \t\n";
	int x = 0;
	while(fgets(line, sizeof(line), mimes)!=0)
	{	

		result = strtok( line , dels);
		if((strcmp(result,"")==0)||(result==NULL)){
			printf("closing file at %d\n",x);
			fclose(mimes);
			return;
		}
		strcpy(mimeTypes[x].ext,result);

		result = strtok( NULL , dels); 
		if((strcmp(result,"")==0)||(result==NULL)){
			printf("closing file at %d\n",x);
			fclose(mimes);
			return;
		}
		strcpy(mimeTypes[x].mimeType,result);
		
		x++;
	}
	fclose(mimes);
		
}

////////////////////////////////////////////////////////////////////
// returns mime type
////////////////////////////////////////////////////////////////////
char* getContType(char* ext, struct mType *mimeTypes){
	char *contType = "";
	int x = 0;
	while(strcmp(mimeTypes[x].ext,"")!=0){
		if( strcmp(ext, mimeTypes[x].ext)==0 ){
			//printf("MIMETYPE!!:%s\n",mimeTypes[x].mimeType);
			return(mimeTypes[x].mimeType);
		}
		x++;
	}
	
	// unknown mimetype, send it binary
	return("application/bin");
}
////////////////////////////////////////////////////////////////////
// Tells us if the request is a directory or a regular file
////////////////////////////////////////////////////////////////////
int TypeOfFile(char *fullPathToFile) {
	struct stat buf;	/* to stat the file/dir */

        if( stat(fullPathToFile, &buf) != 0 ) {
		perror("stat()");
		fprintf(stderr, "[ERROR] stat() on file: |%s|\n", fullPathToFile);
		fflush(stderr);
                exit(-1);
        }


        if( S_ISREG(buf.st_mode) ) {
		return(REG_FILE);
	} else if( S_ISDIR(buf.st_mode) ) { 
	
		return(DIRECTORY);
	}

	return(ERROR_FILE);
}


////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////
void SendDataBin(char *fileToSend, int sock, char *home, char *content, struct mType *mimeTypes) {
        int f;
        int localFile, bytesRead, bytesWritten, fileSize;
	char fullPathToFile[256];
	char fileBuf[1024];
	char Header[8192];
        int s;
        char tempBuf[8192];
        char htmlBuf[8192];
        char pathBuf[255];
	int fret;		/* return value of TypeOfFile() */
	
	struct dirent **dirls;
	int fcnt;
	
	int bytesTotal;

	/*
	 * Build the header
	 */
	//strcpy(Header, "HTTP/1.0 200 OK\r\nContent-length: 112032\r\nContent-type: text/html\r\n\r\n");



	/*
	 * Build the full path to the file
	 */
	sprintf(fullPathToFile, "%s/%s/%s", home, content, fileToSend);
	printf("%s -- %s -- %s\n", home, content, fileToSend);


	
	/*
	 * - If the requested file is a directory, append the 'index.html' 
    	 *   file to the end of the fullPathToFile 
	 *   (Use TypeOfFile(fullPathToFile))
	 * - If the requested file is a regular file, do nothing and proceed
	 * - else your client requested something other than a directory 
	 *   or a reqular file
	 */
	/* TODO 5 */
	if(TypeOfFile(fullPathToFile)==DIRECTORY) {
		strcpy(tempBuf,fullPathToFile);
		strcat(tempBuf,"/index.html");
		if( access(tempBuf, F_OK ) != -1 ) {
			strcat(fullPathToFile,"/index.html");
		} else {
			//strcpy(fullPathToFile,"/home/madmaze/www2/files/crunchbang");
			printf("index.html doesnt exist\n");
			strcpy(htmlBuf,"<!-- generated by maze_httpd_light -->\n<HTML>");
			fcnt = scandir(fullPathToFile, &dirls, 0, alphasort);
			if(fcnt>0){
				s=1;//start at one skipps "."
				while(s<fcnt){
					printf("%s\n", dirls[s]->d_name);
					strcpy(pathBuf, fullPathToFile+(int)strlen(home)+(int)strlen(content)+2);
					if(strcmp(pathBuf+(int)strlen(pathBuf)-1,"/") != 0)
						strcat(pathBuf, "/"); //add trailing slash if missing
					
					strcat(pathBuf, dirls[s]->d_name);
					sprintf(tempBuf, "<a href=\"%s\"><li>%s</li></a>\n", pathBuf, dirls[s]->d_name);
					strcat(htmlBuf, tempBuf);
					s++;
				}
			} else {
				printf("Error reading dir %s\n", fullPathToFile);
			}
			
			strcat(htmlBuf, "</HTML>");
			sprintf(Header, "HTTP/1.0 200 OK\r\nContent-length: %d\r\nContent-type: text/html\r\n\r\n", (int)strlen(htmlBuf));
			strcat(Header, htmlBuf);

			printf("%s\n", Header);
			// Write header
			/*
			if (write(sock, Header, strlen(Header)) <= 0) {
				perror("write");
				exit(-1);
			}*/
			// using send is more reliable
			if (send(sock, Header, strlen(Header), 0) <= 0) {
				perror("write");
				exit(-1);
			}
			return;
		}

	}



	/*
	 * 1. Send the header (use write())
	 * 2. open the requested file (use open())
	 * 3. now send the requested file (use write())
	 * 4. close the file (use close())
	 */
	/* if (write(sock, Header, strlen(Header)) <= 0) {
			perror("write");
			exit(-1);
	 } 
	 Moved to incorporate file size
	 */ 
	
	printf("opening localFile: %s\n",fullPathToFile);
	if( (localFile = open(fullPathToFile, O_RDONLY)) == -1){
		printf("error reading file: %s\n",fullPathToFile);
		// write error
	} else {
		
		fileSize=lseek(localFile, (off_t)0, SEEK_END);
		lseek(localFile,0,SEEK_SET);
		
		printf("FileType: %s,%s,%d\n",getExt(fullPathToFile),getContType(getExt(fullPathToFile), mimeTypes),fileSize);
		
		sprintf(Header,"HTTP/1.0 200 OK\r\nContent-length: %d\r\nContent-type: %s\r\n\r\n", fileSize,getContType(getExt(fullPathToFile),mimeTypes));
		
		printf("%s\n",Header);
		// Write header
		/*if (write(sock, Header, strlen(Header)) <= 0) {
			perror("write");
			exit(-1);
		}*/
		// using send is more reliable
		if (send(sock, Header, strlen(Header), 0) <= 0) {
			perror("write");
			exit(-1);
		}
		
		// Read and Write file
		bytesTotal=0;
		while(1==1){	
	
			bytesRead = read(localFile, fileBuf, sizeof(fileBuf));
			
			// could also be checking for EOF
			if(bytesRead == 0){			
				printf("reached end of file..\n");
				//printf("Stats: Read/Wrote %i bytes with %i separate reads/writes\n",totalbytes,itr);
				break;
			}
			if(bytesRead < 0){
				printf("error reading file: %s\n",fullPathToFile);
				// write error
				break;
			}
	
			// write to socket
			//bytesWritten = write(sock, fileBuf, bytesRead);
			// using send is more reliable with large files than write()
			bytesWritten = send(sock, fileBuf, bytesRead, 0);
			
			bytesTotal+=bytesWritten;
			printf("wrote: %d/%d\n", bytesTotal, fileSize);
			if(bytesWritten != bytesRead){
				printf("error reading file: %s\n",fullPathToFile);
				// write error
				break;
			}
		}	
	}

}


////////////////////////////////////////////////////////////////////
// Extract the file request from the request lines the client sent 
// to us.  Make sure you NULL terminate your result.
////////////////////////////////////////////////////////////////////
void ExtractFileRequest(char *req, char *buff) {
	char *result = NULL;
	const char dels[] = " ";
	
	printf("\nRequest:\n%s\n",buff);
	fflush(stdout);
	result = strtok( buff , dels); // request type GET/POST/PUT
	result = strtok( NULL , dels); // request file
	
	//printf( "extracted req: |%s|\n", result );
	
	strcpy(req,result);
	strcat(req,"\0");
	/* TODO 4  */
}

////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////
int main(int argc, char **argv, char **environ) {
  	pid_t pid;		/* pid of child */
	int sockid;		/* our initial socket */
	int PORT;		/* Port number, used by 'bind' */
	char content[128];	/* Your directory that contains your web 
				   content such as .www in 
				   your home directory */
	char myhome[128];	/* Your home directory */
				/* (gets filled in by GetMyHomeDir() */
	/* 
	 * structs used for bind, accept..
	 */
  	struct sockaddr_in server_addr, client_addr; 

	char file_request[256];	/* where we store the requested file name */
        int one=1;		/* used to set socket options */
        struct mType mimeTypes[700];   
        readMimeFile(mimeTypes);
        
	/* 
	 * Get my home directory from the environment 
	 */
	GetMyHomeDir(myhome, environ);	

	if( argc != 3 ) {
		fprintf(stderr, "USAGE: %s <port number> <content directory>\n", 
								argv[0]);
		exit(-1);
	}

	PORT = atoi(argv[1]);		/* Get the port number */
	strcpy(content, argv[2]);	/* Get the content directory */


	if ( (pid = fork()) < 0) {
		perror("Cannot fork (for deamon)");
		exit(0);
  	}
	else if (pid != 0) {
		/*
	  	 * I am the parent
		 */
		char t[128];
		sprintf(t, "echo %d > %s.pid\n", pid, argv[0]);
		system(t);
    		exit(0);
  	}

	/*
	 * Create our socket, bind it, listen
	 */

	/* TODO 1 */


	signal(SIGCHLD, SIG_IGN);
		/* 
	 * Open a TCP socket. 
	 */
  	if( (sockid = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    		perror("socket");
		exit(-1);
	}
	/* 
	 * To open a UDP socket:    
	 *	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
	 */

  	/* 
	 * Set up structures to bind the address to the socket. 
	 */
  	bzero(&server_addr, sizeof(server_addr));
  	server_addr.sin_family = AF_INET;

        /*
         * INADDR_ANY says that the operating system may choose to 
	 * which local IP address to attach the application.  
	 * For most machines, which only have one address,
         * this simply chooses that address.  The htonl() function converts 
	 * a four-byte integer into the network byte order so that other 
         * hosts can interpret the integer even if they internally store 
	 * integers using a different byte order.
         */
  	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  	server_addr.sin_port = htons(PORT);

	/* 
	 * bind the socket to a well-known port 
	 */
  	if (bind(sockid, (struct sockaddr *) &server_addr, sizeof(server_addr)) != 0) {
		perror("can't bind to socket");
		exit(-1);
	}

	if (listen(sockid, QUEUELENGTH) < 0) {
    		perror("listen");
		exit(-1);
	}


	/* 
	 * - accept a new connection and fork.
	 * - If you are the child process,  process the request and exit.
	 * - If you are the parent close the socket and come back to 
         *   accept another connection
	 */
  	while (1) {
		/* 
		 * socket that will be used for communication
		 * between the client and this server (the child) 
		 */
		int newsock;		

		/*
		 * Get the size of this structure, could pass NULL if we
		 * don't care about who the client is.
		 */
   		int client_len = sizeof(client_addr);

		/*
		 * Accept a connection from a client (a web browser)
		 * accept the new connection. newsock will be used for the 
		 * child to communicate to the client (browser)
		 */
		 /* TODO 2 */
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
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
		else if( pid == 0 ) {
			/*
			 * I am the Child
			 */
			int r;
      			char buff[1024];
			int read_so_far = 0;
			int bytesRcvd;
			char ref[1024], rowRef[1024];

			close(sockid);

			memset(buff, 0, 1024);

			/*
			 * Read client request into buff
			 * 'use a while loop'
			 */
			/* TODO 3 */
			// read back return
			if ((bytesRcvd = read(newsock, buff, RCVBUFSIZE - 1)) <= 0) {
				perror("read");
				exit(-1);
			}
			
			ExtractFileRequest(file_request, buff);

			printf("** File Requested: |%s|\n", file_request);
			fflush(stdout);

			SendDataBin(file_request, newsock, myhome, content, mimeTypes);
			shutdown(newsock, 1);
			close(newsock);
			printf("\n");
			exit(0);
    		}
		/*
		 * I am the Parent
		 */
		close(newsock);	/* Parent handed off this connection to its child,
			           doesn't care about this socket */
  	}
}
