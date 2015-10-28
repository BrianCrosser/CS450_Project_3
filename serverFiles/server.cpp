
/* This is the server for a very simple file transfer
   service.  This is a "concurrent server" that can
   handle requests from multiple simultaneous clients.
   For each client:
   - get file name and check if it exists
   - send size of file to client
   - send file to client, a block at a time
   - close connection with client
*/

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
#include <thread>

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

int writen(int sd,char *ptr,int size);
int readn(int sd,char *ptr,int size);
void doftp(int newsd);
void doctp(int newsd);

int main()  {
    int sockid, newsd, pid; 
    struct sockaddr_in my_addr, client_addr;   
    socklen_t clilen = sizeof( client_addr );

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
    for( ;; ) { 
        /* Accept a connection and then create a child to do the work */
        /* loop back and wait for another connection                  */

        printf("server: starting accept\n");
        if ((newsd = accept(sockid ,(struct sockaddr *) &client_addr, &clilen)) < 0) {
            perror( "Failed to accept" );
            exit(0); 
        }
        printf("server: return from accept, socket for this ftp: %d\n", newsd);
        if ( (pid=fork()) == 0) {
            /* Child proc starts here. it will do actual file transfer */
            close(sockid);   /* child shouldn't do an accept */
            //doftp(newsd);
			doctp(newsd);
            close (newsd);
            exit(0);         /* child all done with work */
        }
        /* Parent continues below here */
        close(newsd);    /* parent all done with client, only child */
    }                  /* will communicate with that client from now on */
}   
     

/* Child procedure, which actually does the file transfer */
void doftp(int newsd)
{       
    int i,fsize,fd,msg_ok,fail,fail1,req,c,ack;
    int no_read ,num_blks , num_blks1,num_last_blk,num_last_blk1,tmp;
    char fname[MAXLINE], fnameLen = 0;
    char out_buf[MAXSIZE];
    FILE *fp;
      
    no_read = 0;
    num_blks = 0;
    num_last_blk = 0; 

    
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
    if (req!=REQUESTFILE) {
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
  
    fail = FILENAMEOK;
    if((fnameLen = read(newsd,fname,MAXLINE)) < 0) {
        perror("Filename read error" );
        fail = BADFILENAME ;
    }
    fname[ fnameLen ] = '\0';
   
    /* If server cant open file then inform client of this and terminate */
    if((fp = fopen(fname, "r")) == NULL) /*cant open file*/
        fail = BADFILENAME;

    tmp = htons(fail);
    if((writen(newsd,(char *)&tmp,sizeof(tmp))) < 0) {
        perror("Write error" );
        exit(0);   
    }
    if(fail == BADFILENAME) {
        printf("server cant open file\n");
        close(newsd);
        exit(0);
    }
    printf("server: filename is %s\n",fname);
  
    req = 0;
    if ( readn(newsd,(char *)&req,sizeof(req)) < 0) {
        perror( "Read error" );
        exit(0);
    }
    printf("server: start transfer command, %d, received\n", ntohs(req));

   
    /*Server gets filesize and calculates the number of blocks of 
      size = maxsize it will take to transfer the file. Also calculate
      number of bytes in the last partially filled block if any.  Send
      this info to client, receiving acks */

    printf("server: starting transfer\n");
    fsize = 0;ack = 0;   
    while ((c = getc(fp)) != EOF) {fsize++;}
    num_blks = fsize / MAXSIZE; 
    num_blks1 = htons(num_blks);
    num_last_blk = fsize % MAXSIZE; 
    num_last_blk1 = htons(num_last_blk);
    if((writen(newsd,(char *)&num_blks1,sizeof(num_blks1))) < 0) {
        perror( "Write error" );
        exit(0);
    }
    printf("server: told client there are %d blocks\n", num_blks);  
    if((readn(newsd,(char *)&ack,sizeof(ack))) < 0) {
        perror("ack read error " );
        exit(0); 
    }          
    if (ntohs(ack) != ACK) {
        printf("client: ACK not received on file size\n");
        exit(0);
    }
    if((writen(newsd,(char *)&num_last_blk1,sizeof(num_last_blk1))) < 0) {
        perror( "write error" );
        exit(0);
    }
    printf("server: told client %d bytes in last block\n", num_last_blk);  
    if( readn(newsd,(char *)&ack,sizeof(ack)) < 0) {
        perror( "ack read error" );
        exit(0); 
    }
    if (ntohs(ack) != ACK) {
        printf("server: ACK not received on file size\n");
        exit(0);
    }
    rewind(fp);    
  
    /* Actual file transfer starts  block by block*/       
       
    for(i= 0; i < num_blks; i ++) { 
        no_read = fread(out_buf,sizeof(char),MAXSIZE,fp);
        if (no_read == 0) {
            printf("server: file read error\n");
            exit(0);
        }
        if (no_read != MAXSIZE) {
            printf("server: file read error : no_read is less\n");
            exit(0);
        }
        if((writen(newsd,out_buf,MAXSIZE)) < 0) {
            perror("Write error: sending block" );
            exit(0);
        }
        if((readn(newsd,(char *)&ack,sizeof(ack))) < 0) {
            perror("ack read  error" );
            exit(0);
        }
        if (ntohs(ack) != ACK) {
            printf("server: ACK not received for block %d\n",i);
            exit(0);
        }
        printf(" %d...",i);
    }

    if (num_last_blk > 0) { 
        printf("%d\n",num_blks);
        no_read = fread(out_buf,sizeof(char),num_last_blk,fp); 
        if (no_read == 0) {
            printf("server: file read error\n");
            exit(0);
        }
        if (no_read != num_last_blk) {
            printf("server: file read error : no_read is less 2\n");
            exit(0);
        }
        if((writen(newsd,out_buf,num_last_blk)) < 0) {
            perror("File transfer error" );
            exit(0);
        }
        if((readn(newsd,(char *)&ack,sizeof(ack))) < 0) {
            perror("ack read  error" );
            exit(0);
        }
        if (ntohs(ack) != ACK) {
            printf("server: ACK not received last block\n");
            exit(0);
        }
    }
    else printf("\n");
                                                  
    /* FILE TRANSFER ENDS */
    printf("server: FILE TRANSFER COMPLETE on socket %d\n",newsd);
    fclose(fp);
    close(newsd);
}

/* Child procedure, which actually does the command transfer */
void doctp(int newsd)
{       
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
    printf("server: command is %s\n",command);
  
    /*
	req = 0;
    if ( readn(newsd,(char *)&req,sizeof(req)) < 0) {
        perror( "Read error" );
        exit(0);
    }
    printf("server: start transfer command, %d, received\n", ntohs(req));

   
    /*Server gets filesize and calculates the number of blocks of 
      size = maxsize it will take to transfer the file. Also calculate
      number of bytes in the last partially filled block if any.  Send
      this info to client, receiving acks 

    printf("server: starting transfer\n");
    fsize = 0;ack = 0;   
    while ((c = getc(fp)) != EOF) {fsize++;}
    num_blks = fsize / MAXSIZE; 
    num_blks1 = htons(num_blks);
    num_last_blk = fsize % MAXSIZE; 
    num_last_blk1 = htons(num_last_blk);
    if((writen(newsd,(char *)&num_blks1,sizeof(num_blks1))) < 0) {
        perror( "Write error" );
        exit(0);
    }
    printf("server: told client there are %d blocks\n", num_blks);  
    if((readn(newsd,(char *)&ack,sizeof(ack))) < 0) {
        perror("ack read error " );
        exit(0); 
    }          
    if (ntohs(ack) != ACK) {
        printf("client: ACK not received on file size\n");
        exit(0);
    }
    if((writen(newsd,(char *)&num_last_blk1,sizeof(num_last_blk1))) < 0) {
        perror( "write error" );
        exit(0);
    }
    printf("server: told client %d bytes in last block\n", num_last_blk);  
    if( readn(newsd,(char *)&ack,sizeof(ack)) < 0) {
        perror( "ack read error" );
        exit(0); 
    }
    if (ntohs(ack) != ACK) {
        printf("server: ACK not received on file size\n");
        exit(0);
    }
    rewind(fp);    
  
    /* Actual file transfer starts  block by block       
       
    for(i= 0; i < num_blks; i ++) { 
        no_read = fread(out_buf,sizeof(char),MAXSIZE,fp);
        if (no_read == 0) {
            printf("server: file read error\n");
            exit(0);
        }
        if (no_read != MAXSIZE) {
            printf("server: file read error : no_read is less\n");
            exit(0);
        }
        if((writen(newsd,out_buf,MAXSIZE)) < 0) {
            perror("Write error: sending block" );
            exit(0);
        }
        if((readn(newsd,(char *)&ack,sizeof(ack))) < 0) {
            perror("ack read  error" );
            exit(0);
        }
        if (ntohs(ack) != ACK) {
            printf("server: ACK not received for block %d\n",i);
            exit(0);
        }
        printf(" %d...",i);
    }

    if (num_last_blk > 0) { 
        printf("%d\n",num_blks);
        no_read = fread(out_buf,sizeof(char),num_last_blk,fp); 
        if (no_read == 0) {
            printf("server: file read error\n");
            exit(0);
        }
        if (no_read != num_last_blk) {
            printf("server: file read error : no_read is less 2\n");
            exit(0);
        }
        if((writen(newsd,out_buf,num_last_blk)) < 0) {
            perror("File transfer error" );
            exit(0);
        }
        if((readn(newsd,(char *)&ack,sizeof(ack))) < 0) {
            perror("ack read  error" );
            exit(0);
        }
        if (ntohs(ack) != ACK) {
            printf("server: ACK not received last block\n");
            exit(0);
        }
    }
    else printf("\n");
                                                  
    /* FILE TRANSFER ENDS 
	*/
    printf("server: Command TRANSFER COMPLETE on socket %d\n",newsd);
    close(newsd);
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
