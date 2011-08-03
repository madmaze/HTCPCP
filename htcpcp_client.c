#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 

void error(const char *msg)
{
    perror(msg);
    exit(0);
}

int main(int argc, char *argv[])
{
    int sockfd, portno, n, cmd, pot;
    struct sockaddr_in serv_addr;
    struct hostent *server;

    char buffer[1024];
    if (argc < 5) {
       fprintf(stderr,"usage %s hostname port cmd pot\n", argv[0]);
       exit(0);
    }
    portno = atoi(argv[2]);
    cmd = atoi(argv[3]);
    pot = atoi(argv[4]);
    printf("Choice %d\n",cmd);
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");
    server = gethostbyname(argv[1]);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, 
         (char *)&serv_addr.sin_addr.s_addr,
         server->h_length);
    serv_addr.sin_port = htons(portno);
    if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) 
        error("ERROR connecting");
    printf("Sending request: ");
    bzero(buffer,1024);
    
    
    if(cmd==0){
    	    sprintf(buffer,"BREW /pot-%d HTCPCP/1.0\r\nContent-Type: message/coffeepot\r\nAccept-Additions: cream;1,whisky;3,rum;5\r\n",pot);
    	    printf("%s\n",buffer);
    } else if(cmd==1){
    	    sprintf(buffer,"PUT /pot-%d HTCPCP/1.0 \r\n",pot);
    	    printf("%s\n",buffer);
    } else if(cmd==2){
    	    sprintf(buffer,"WHEN /pot-%d HTCPCP/1.0\r\n",pot);
    	    printf("%s\n",buffer);
    } else if(cmd==3){
    	    sprintf(buffer,"GET /pot-%d HTCPCP/1.0 \r\n",pot);
    	    printf("%s\n",buffer);
    } 
    
    //strcpy(buffer,"BREW /pot-4 HTCPCP/1.0\r\nHost: 120.0.0.1\r\nContent-Type: message/coffeepot\r\nAccept-Additions: cream;1,whisky;3,rum;5\r\n");
    //strcpy(buffer,"PUT /pot-4 HTCPCP/1.0 \r\n");
    n = write(sockfd,buffer,strlen(buffer));
    if (n < 0) 
         error("ERROR writing to socket");
    bzero(buffer,1024);
    n = read(sockfd,buffer,1024);
    if (n < 0) 
         error("ERROR reading from socket");
    printf("%s\n",buffer);
    close(sockfd);
    return 0;
}
