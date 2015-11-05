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
#include <thread>
#include <iostream>

#define MY_PORT_ID 8060
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

using namespace std;

int writen(int sd,char *ptr,int size);
int readn(int sd,char *ptr,int size);
void getUserInput(string & userCommand);
void giveCommand(int newsd);
void receiveCommand(int newsd);

int main()  {
    int sockid, newsd; 
    struct sockaddr_in my_addr, client_addr;
    socklen_t clilen = sizeof(client_addr);
    printf("server: creating socket\n");
    if ((sockid = socket(AF_INET,SOCK_STREAM,0)) < 0) {
        printf("server: socket error : %d\n", errno); 
        exit(0); 
    }

    printf("server: binding my local socket\n");
    bzero((char *) &my_addr,sizeof(my_addr));
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(MY_PORT_ID);
    my_addr.sin_addr.s_addr = htons(INADDR_ANY);
    if (bind(sockid ,(struct sockaddr *) &my_addr,sizeof(my_addr)) < 0) {
        perror( "Bind error" );
        exit(0); 
    }
    printf("server: starting listen \n");
    if (listen(sockid,5) < 0) {
        perror( "Listen error" );
        exit(0);
    }                                        
    
    //thread giveCmd (giveCommand, newsd);

    while( true ) {
        printf("server: starting accept\n");
	    if ((newsd = accept(sockid ,(struct sockaddr *) &client_addr, &clilen)) < 0) {
    	    perror( "Failed to accept" );
	        exit(0); 
	    }
                
        thread receiveCmd (receiveCommand, newsd);
        receiveCmd.detach();
    }
		
}   
     

void receiveCommand(int newsd)
{
    string cmd;
    while(cmd != "gbye"){
    	int msg_ok,fail,req;
    int tmp;
    char command[MAXLINE], commandLen = 0;
    
    
    /* Start servicing the client */ 
  
    /* get command code from client.*/
    /* only one supported command: 100 -  get a file */
    req = 0;
    if((readn(newsd,(char *)&req,sizeof(req))) < 0) {
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
        if((writen(newsd,(char *)&msg_ok,sizeof(msg_ok))) < 0) {
            perror( "Write error" );
            exit(0);
        }
        exit(0);
    }

    /* reply to client: command OK  (code: 160) */
    msg_ok = COMMANDSUPPORTED; 
    msg_ok = htons(msg_ok);
    if( writen(newsd,(char *)&msg_ok,sizeof(msg_ok))  < 0) {
        perror( "Write error" );
        exit(0);
    }
  
    fail = COMMANDOK;
    if((commandLen = read(newsd,command,MAXLINE)) < 0) {
        perror("Command read error" );
        fail = BADCOMMAND ;
    }
    command[ commandLen ] = '\0';
    cmd = command;  
    tmp = htons(fail);
    if((writen(newsd,(char *)&tmp,sizeof(tmp))) < 0) {
        perror("Write error" );
        exit(0);   
    }
    if(fail == BADCOMMAND) {
        printf("server cant do command");
        close(newsd);
        exit(0);
    }
    printf("server: command is: %s\n",command);
  
    printf("server: Command TRANSFER COMPLETE on socket %d\n",newsd);
    }
    close(newsd);
}

void giveCommand(int newsd)
{
	string userCommand;
    int getcommand,msg,msg_2,len;
	
	getUserInput(userCommand);
    
	while(userCommand != "shutdown"){
        len = userCommand.size();

	    getcommand = htons(REQUESTCOMMAND);
        printf("client: sending command request to ftp server\n");
        if((writen(newsd,(char *)&getcommand,sizeof(getcommand))) < 0) {
            printf("client: write  error :%s\n",  strerror( errno )); 
            exit(0);
        } 

        /* want for go-ahead from server */
        msg = 0;  
        if((readn(newsd,(char *)&msg,sizeof(msg)))< 0) {
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
        if ((writen(newsd, (char *)userCommand.c_str(),len))< 0) {
            printf("client: write  error :%s\n",  strerror( errno )); 
            exit(0);
        }
        /* see if server replied that file name is OK */
        msg_2 = 0;
        if (readn(newsd, (char *)&msg_2, sizeof(msg_2)) < 0) {
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
    //close(newsd);
}

/*
  To take care of the possibility of buffer limmits in the kernel for the
  socket being reached (which may cause read or write to return fewer characters
  than requested), we use the following two functions */  
   
int readn(int sd,char *ptr,int size)
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
