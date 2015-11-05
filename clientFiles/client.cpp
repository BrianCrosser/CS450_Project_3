#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <strings.h>
#include <string.h>
#include <string>
#include <iostream>
#include <thread>

#define SERVER_PORT_ID 8060
#define CLIENT_PORT_ID 8061

#define MAXLINE 256
#define MAXSIZE 512

#define ACK                   2
#define NACK                  3
#define REQUESTFILE           100
#define REQUESTCOMMAND		  110
#define COMMANDNOTSUPPORTED   150
#define COMMANDSUPPORTED      160
#define BADFILENAME           200
#define BADCOMMAND			  210
#define FILENAMEOK            400
#define COMMANDOK			  410
#define STARTTRANSFER         500

using namespace std;

int readn(int sd,char *ptr,int size);
int writen(int sd,char *ptr,int size);
void getUserInput(string & userCommand);
void receiveCommand(int newsd);
void giveCommand(int sockid);

int main(int argc,char *argv[])
{
	int sockid, newsd;
    struct sockaddr_in my_addr, server_addr; 
    
    if(argc != 2) {
        printf("error: usage : %s IP-dotted-notation\n", argv[0]); 
        exit(0);
    }
     
    printf("client: creating socket\n");
    if ((sockid = socket(AF_INET,SOCK_STREAM,0)) < 0)  { 
        printf("client: socket error : %s\n",  strerror( errno )); 
        exit(0);
    }
  
    printf("client: binding my local socket\n");
    bzero((char *) &my_addr, sizeof(my_addr));
    my_addr.sin_family = AF_INET;
    my_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    my_addr.sin_port = htons(CLIENT_PORT_ID);
    if (bind(sockid ,(struct sockaddr *) &my_addr,sizeof(my_addr)) < 0) {
        printf("client: bind  error :%s\n",  strerror( errno )); 
        exit(0);
    }

	printf("client: starting connect\n");
    bzero((char *) &server_addr,sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(argv[1]);
    server_addr.sin_port = htons(SERVER_PORT_ID);
	if (connect(sockid ,(struct sockaddr *) &server_addr,
                sizeof(server_addr)) < 0) {
        printf("client: connect  error :%s\n",  strerror( errno )); 
        exit(0);
    }
    cout << "Before thread" << endl;
    //thread giveCmd (giveCommand, sockid);
    giveCommand(sockid);
    cout << "After thread" << endl;
	//giveCmd.detach();

	/*
    while( true ) {
        printf("server: starting accept\n");
	    if (connect(sockid ,(struct sockaddr *) &server_addr,
                sizeof(server_addr)) < 0) {
        printf("client: connect  error :%s\n",  strerror( errno )); 
        exit(0);
    }
                
        thread receiveCmd (receiveCommand, newsd);
        receiveCmd.detach();
    }
	*/
}            

void receiveCommand(int sockid)
{
	int msg_ok,fail,req;
    int tmp;
    char command[MAXLINE], commandLen = 0;
    
    
    /* Start servicing the client */ 
  
    /* get command code from client.*/
    /* only one supported command: 100 -  get a file */
    req = 0;
    if((readn(sockid,(char *)&req,sizeof(req))) < 0) {
        perror( "Read error" );
        exit(0);
    }
    req = ntohs(req);
    printf("server: client request code is: %d\n",req);
    if (req!=REQUESTCOMMAND) {
        printf("server: unsupported operation. goodbye\n");
        /* reply to client: command not OK  (code: 150) */
        msg_ok = COMMANDNOTSUPPORTED; 
        msg_ok = htons(msg_ok);
        if((writen(sockid,(char *)&msg_ok,sizeof(msg_ok))) < 0) {
            perror( "Write error" );
            exit(0);
        }
        exit(0);
    }

    /* reply to client: command OK  (code: 160) */
    msg_ok = COMMANDSUPPORTED; 
    msg_ok = htons(msg_ok);
    if( writen(sockid,(char *)&msg_ok,sizeof(msg_ok))  < 0) {
        perror( "Write error" );
        exit(0);
    }
  
    fail = COMMANDOK;
    if((commandLen = read(sockid,command,MAXLINE)) < 0) {
        perror("Command read error" );
        fail = BADCOMMAND ;
    }
    command[ commandLen ] = '\0';
   
    tmp = htons(fail);
    if((writen(sockid,(char *)&tmp,sizeof(tmp))) < 0) {
        perror("Write error" );
        exit(0);   
    }
    if(fail == BADCOMMAND) {
        printf("server cant do command");
        close(sockid);
        exit(0);
    }
    printf("server: command is %s\n",command);
  
    printf("server: Command TRANSFER COMPLETE on socket %d\n",sockid);
    close(sockid);
	
}

void giveCommand(int sockid)
{
    cout << "Check Point Reached!" << endl;
	string userCommand;
	
	getUserInput(userCommand);
    
	while(userCommand != "shutdown"){
        int getcommand,msg,msg_2,len;
        len = userCommand.size();

	    getcommand = htons(REQUESTCOMMAND);
        printf("client: sending command request to ftp server\n");
        if((writen(sockid,(char *)&getcommand,sizeof(getcommand))) < 0) {
            printf("client: write  error :%s\n",  strerror( errno )); 
            exit(0);
        } 

        /* want for go-ahead from server */
        msg = 0;  
        if((readn(sockid,(char *)&msg,sizeof(msg)))< 0) {
            printf("client: read  error :%s\n",  strerror( errno )); 
         exit(0); 
        }
        msg = ntohs(msg);   
        if (msg==COMMANDNOTSUPPORTED) {
            printf("client: server refused command. goodbye\n");
            exit(0);
        } else
            printf("client: server replied %d, command supported\n", msg);

        /* send file name to server */
        printf("client: sending command\n");
        if ((writen(sockid, (char *)userCommand.c_str(),len))< 0) {
            printf("client: write  error :%s\n",  strerror( errno )); 
            exit(0);
        }
        /* see if server replied that file name is OK */
        msg_2 = 0;
        if (readn(sockid, (char *)&msg_2, sizeof(msg_2)) < 0) {
            printf("client: read  error :%s\n",  strerror( errno )); 
            exit(0); 
        }   
        msg_2 = ntohs(msg_2);
        if (msg_2 == BADCOMMAND) {       
            printf("client: server reported bad command. goodbye.\n");
            exit(0);
        } else
            printf("client: server replied %d, filename OK\n",msg_2);
    

        /*File transfer ends. client terminates after  closing all its files
          and sockets*/ 
        printf("client: Command TRANSFER COMPLETE\n");
        getUserInput(userCommand);
    }
    //close(sockid);
}

int readn(int sd, char *ptr, int size)
{
    int no_left,no_read;
    no_left = size;
    while (no_left > 0) { 
        no_read = read(sd,ptr,no_left);
        if(no_read <0)  return(no_read);
        if (no_read == 0) break;
        no_left -= no_read;
        ptr += no_read;
    }
    return(size - no_left);
}


int writen(int sd,char *ptr,int size)
{         int no_left,no_written;
    no_left = size;
    while (no_left > 0) { 
        no_written = write(sd,ptr,no_left);
        if(no_written <=0)  return(no_written);
        no_left -= no_written;
        ptr += no_written;
    }
    return(size - no_left);
}


void getUserInput(string & userCommand)
{
    cout << "~: ";
    getline(cin, userCommand);
}
