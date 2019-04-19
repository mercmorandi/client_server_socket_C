/*
 * TEMPLATE
 */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <stdint.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <string.h>
#include <time.h>
#include <unistd.h>

#include "../sockwrap.h"
#include "../errlib.h"

#define MTU 65536
#define ERR_MSG "-ERR\r\n"

char *prog_name;

int prot_get(int s, const char *buffer)
{
  char *filename;
  FILE *fp;
  char *buffer2;
  uint32_t dimf, lastM;
  int byteR;
  struct stat fileInfo;
  size_t sent;

  filename = malloc(strlen(buffer) + 2);
  int i = 4;
  int j = 0;
  while (buffer[i] != '\r')
  {
    memcpy(&filename[j], &buffer[i], 1);
    i++;
    j++;
  }
  filename[j] = 0;

  if ((fp = fopen(filename, "rb")) == NULL)
  {
    printf("error in fopen()\n");
    Write(s, ERR_MSG, strlen(ERR_MSG));
    close(s);
    memset(filename, 0, strlen(filename));
    free(filename);
    return -1;
  }
  printf("fopen success\n");
  if (stat(filename, &fileInfo) != 0)
    printf("error in reading file stat\n");
  lastM = fileInfo.st_mtime;
  dimf = fileInfo.st_size;

  Write(s, "+OK\r\n", strlen("+OK\r\n"));
  dimf = htonl(dimf);
  lastM = htonl(lastM);
  Write(s, &dimf, sizeof(dimf));
  Write(s, &lastM, sizeof(lastM));

  buffer2 = malloc(MTU);
  int transfered = 0;
  while (transfered < fileInfo.st_size)
  {
    int rd = fread(buffer2, 1, MTU, fp);
    if (Write(s, buffer2, rd) == -1)
    {
      printf("--- write failed\n");
      fclose(fp);
      close(s);
      memset(buffer2, 2, strlen(buffer2));
      free(buffer2);
      memset(filename, 0, strlen(filename));
      free(filename);
      return -1;
    }
    //  sleep(6);
    transfered += rd;
  }
  fclose(fp);
  printf("--- transfered %s of %d bytes\n\n", filename, transfered);

  memset(buffer2, 0, strlen(buffer2));
  free(buffer2);
  memset(filename, 0, strlen(filename));
  free(filename);

  return 0;
}

void newReq(int s)
{
  char buffer[255];
  ssize_t dim;
  dim = Read(s, buffer, sizeof(buffer));

  buffer[dim] = '\0';

  if (dim == 0)
  {
    printf("--- error in read()\n");
    return;
  }

  printf("--- %s\n", buffer);

  while (buffer[0] != 'Q')
  {
    int ret = prot_get(s, (char *)&buffer);
    memset(buffer, 0, strlen(buffer));
    if (ret == -1)
      return;
    dim = Read(s, buffer, sizeof(buffer));
    buffer[dim] = '\0';
    printf("--- %s", buffer);
    if (dim == 0)
    {
      printf("error in recvrom\n");
      return;
    }
  }
  memset(buffer, 0, strlen(buffer));
  return;
}

int main(int argc, char *argv[])
{

  uint16_t port;
  struct sockaddr_in saddr, caddr;
  int s1, s2;
  socklen_t caddrlen = sizeof(caddr);
  prog_name = argv[0];
  port = htons(atoi(argv[1]));
  char addr[32];
  strcpy(addr, "0.0.0.0");
  SetAddress(addr, port, &saddr);

  s1 = InitServerTCP((SA *)&saddr);
  printf("---socket created\nlistening on %s:%u\n", inet_ntoa(saddr.sin_addr), ntohs(saddr.sin_port));
  while (1)
  {
    printf("-----------------------------------------------------------waiting for connections ....\n");
    s2 = Accept(s1, (SA *)&caddr, &caddrlen);
    if (s2 < 0)
    {
      printf("error accept() failed\n");
      return 1;
    }
    printf("-----------------------------------------------------------new connection from client %s:%u\non socket %d\n", inet_ntoa(caddr.sin_addr), ntohs(caddr.sin_port), s2);
    int childpid = fork();
    if (childpid < 0)
      err_ret("fork() failed");
    else if (childpid > 0)
    {
      //father
      close(s2);
    }
    else
    {
      //child
      close(s1);
      newReq(s2);
      exit(0);
    }
  }

  return 0;
}
