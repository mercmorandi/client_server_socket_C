/*
 * TEMPLATE
 */
 #include <stdio.h>
 #include <stdlib.h>
 #include <errno.h>
 #include <ctype.h>
 #include <stdint.h>

 #include <sys/types.h>
 #include <sys/socket.h>
 #include <arpa/inet.h>
 #include <netinet/in.h>


 #include <string.h>
 #include <time.h>
 #include <unistd.h>

#include "../sockwrap.h"
#include "../errlib.h"
#define MTU 65536
#define QUIT_MSG "QUIT\r\n"


char *prog_name;

int main (int argc, char *argv[])
{
	prog_name = argv[0];
	char * fname;
	char * address;
  char * buffer1;
  char * buffer2;
  fd_set cset;
	struct sockaddr_in saddr;
  struct timeval tval;
  char date[80];
  struct tm *tm;
	int s,i,cont;
  ssize_t dim;
  uint32_t dimF;
  uint32_t lastM;
  uint16_t port;
  FILE * fp;

  prog_name = argv[0];
	if(argc<4)
				err_quit("usage: %s <dest_ip> <dest_port> <file_name>\n",prog_name);



  address = strdup(argv[1]);
  port = htons(atoi(argv[2]));
  SetAddress(address,port,&saddr);
  s = InitClientTCP((SA*) &saddr);
  if(s<0){
    printf("-ERROR impossible connect to server\n");
    return -1;
  }
  printf("--- connection up, socket: %d\n\n",s);
  i = 3;
  cont = argc;
  for(i = 3; i<argc; ++i){
	     fname = strdup(argv[i]);
	     buffer1 = malloc((strlen(fname) + 7));
       sprintf(buffer1,"GET %s\r\n",fname);
       buffer1[strlen(buffer1)] = 0;
       Write(s,buffer1,strlen(buffer1));
       printf("--- Downloading %s ....\n",fname);
       memset(buffer1,0,strlen(buffer1));
       free(buffer1);
       buffer2 = malloc(13);
       dim = Read(s,buffer2,5);
       buffer2[dim] = '\0';
       if(buffer2[1] == 'E'){
            printf("error connection closed by server: %s\n",buffer2);
            memset(buffer2,0,strlen(buffer2));
            free(buffer2);
            return -1;
          }

       printf("--- %s",buffer2);
       Read(s,&dimF,4);
       Read(s,&lastM,4);

       dimF = ntohl(dimF);
       lastM = ntohl(lastM);

       time_t t = lastM;
       tm = localtime(&t);
       strftime(date,sizeof(date),"%d.%m.%Y %H:%M:%S", tm);

       memset(buffer2,0,strlen(buffer2));
       free(buffer2);
       buffer2 = malloc(MTU);
       char  path[10+strlen(fname)];
       sprintf(path,"%s",fname);
       if((fp = fopen(path,"wb")) == NULL){
         printf("--- error in fopen\n");
         close(s);
         return -1;
       }

       int recvd = 0;

       while(recvd < dimF){
            tval.tv_sec = 10; //i parametri della select() vanno resetatti ad ogni sua chiamata!!!
            tval.tv_usec = 0;
            FD_ZERO(&cset);
            FD_SET(s,&cset);
            if(Select(FD_SETSIZE,&cset,NULL,NULL,&tval)>0){
                  int r = Read(s,buffer2,MTU);
                  int written = fwrite(buffer2,r,1,fp);
                  if(written != 1){
                    printf("write failed\n");
                    fclose(fp);
                    remove(path);
                    close(s);
                    return -1;
                  }
                  recvd+=r;
                }else{
                  printf("---Timeout expired during file download, closing connection...\n");
                  fclose(fp);
                  remove(path);
                  close(s);
                  return -1;
                }
          }

      fclose(fp);
      printf("--- FILE %s TRANSFERED  file size: %d last modify: %s\n\n",fname,recvd,date);
      memset(tm,0,sizeof(tm));
      memset(date,0,strlen(date));

      memset(fname,0,strlen(fname));
      free(fname);
      memset(buffer2,0,MTU);
      free(buffer2);
}

  Write(s,QUIT_MSG,strlen(QUIT_MSG));
  close(s);
	printf("connection closed\n");
  memset(address,0,strlen(address));
  free(address);

	return 0;
}
