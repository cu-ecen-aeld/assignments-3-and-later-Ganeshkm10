/*
 * @filename: aesdsocket.c
 * @author: Ganesh KM
 * @Description: To build a native server socket for A5-part1
 * @references: https://qemu.readthedocs.io/en/latest/system/devices/net.html
 *              https://beej.us/guide/bgnet/html/#what-is-a-socket 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/queue.h>
#include <pthread.h>
#include <time.h>

#define PORT "9000"
#define bufferlen 1024
#define backlog 10      //pending connections on the listen syscall

static int sockfd = -1;
static int client_sockfd = -1; 
struct addrinfo hints, *serverinfo;
static char buffer[bufferlen];

void close_connections()
{
  shutdown(sockfd, 2);
  close(sockfd);
  shutdown(client_sockfd, 2);
  close(client_sockfd); 
  if(serverinfo != NULL)
  {
    freeaddrinfo(serverinfo) ;
  }
  remove("/var/tmp/aesdsocketdata");
  syslog(LOG_DEBUG, "closed connections\n");
   //close the logging	
  closelog();
}

static void signal_handler(int sig)
{
  syslog(LOG_DEBUG, "Signal caught \n");
  if((sig == SIGINT) || (sig == SIGTERM))
  close_connections();
  exit(0);
}


int main()
{
 openlog("aesdsocket", 0, LOG_USER);

// register signals 
  if(signal(SIGINT, signal_handler) == SIG_ERR) 
  {
    syslog(LOG_ERR, "Error while registering SIGINT\n");
    exit(1);
       
  }
  if (signal(SIGTERM, signal_handler) == SIG_ERR) 
  {
    syslog(LOG_ERR, "Error while registering SIGTERM\n");
    exit(1);   
  }
  
  memset(&hints, 0, sizeof(hints));
  //memset(&buffer, 0, bufferlen);

  hints.ai_family = AF_INET; //IPv4
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;
  int status;

  //int getaddrinfo(const char *node, const char *service, const struct addrinfo *hints, struct addrinfo **res)                 
  if((status = getaddrinfo(NULL, (PORT), &hints, &serverinfo))!=0)
  {
    syslog(LOG_ERR, "Error while registering SIGINT %s\n\r", strerror(errno));
    exit(1);
  }

   // creating a socket fd
   //int socket(int domain, int type, int protocol)
  if((sockfd = socket(serverinfo->ai_family, serverinfo->ai_socktype, 0))< 0)
  {
    printf("socket creation failed \n");
    syslog(LOG_ERR, "socket creation failed %s\n\r", strerror(errno));
    exit(1);
  }

  // Handling the resue of port
  int yes = 1;
  if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0)
  {
    printf("setsockopt error\n");
    syslog(LOG_ERR, "setsockopt %s\n\r", strerror(errno));
    exit(1);
  }
  
  // bind system call
  //int bind(int sockfd, struct sockaddr *my_addr, int addrlen)
  if (bind(sockfd, serverinfo->ai_addr, serverinfo->ai_addrlen) < 0)
  {
    printf("Bind error \n");
    syslog(LOG_ERR, "Bind error: %s\n\r", strerror(errno));
    exit(1);
  }
  
  //int listen(int sockfd, int backlog)
  if(listen(sockfd, backlog)== -1)
  {
    printf ("Listen error \n");
    syslog(LOG_ERR, "error on syscall Listen : %s\n\r", strerror(errno));
    exit(1);
  }
  
  // forking to run the program as daemon
  pid_t pd;
  pd=fork();

  if(pd<0)
    return -1;
  else if(pd>0)
  {
    printf("%d", pd);
    return 0;
  }

  if( setsid() <0)
    return -1;

  signal(SIGCHLD, SIG_IGN);
  signal(SIGHUP, SIG_IGN);

  umask(0);
  chdir("/");

  close(STDIN_FILENO);
  close(STDOUT_FILENO);
  close(STDERR_FILENO); 
 
  while(1)
  {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    // Accept an incoming connection
    client_sockfd = accept(sockfd, (struct sockaddr*)&client_addr, &client_len);
    if(client_sockfd < 0)
    {
      printf("accept error \n");
      close_connections();
    }
    
    char *client_ip = inet_ntoa(client_addr.sin_addr);
    printf(" Accepted connection from %s \n",client_ip);

    // read the data
    while(1)
    {
      // int bytes_read = read(client_sockfd, buffer, bufferlen);
      int bytes_read = recv(client_sockfd, buffer, bufferlen, 0 );
	    if(bytes_read == -1)
	    {
	      printf(" syscall recv error \n");
	      continue; 
	    }
	    if(bytes_read == 0)
	      continue;
      // printf("read %d bytes of data \n",bytes_read);

      int fd = open("/var/tmp/aesdsocketdata",O_RDWR | O_CREAT | O_APPEND, 0766);
      if(fd == -1)
      {
        syslog(LOG_ERR, "Error writing to file %s\n\r", strerror(errno));
      }

      int write_bytes = write(fd, buffer, bytes_read);
      
      if(write_bytes == -1)
      {
        syslog(LOG_ERR, "Error writing to file %s\n\r", strerror(errno));
        close(fd);
        continue;
      }
      else if(write_bytes!=bytes_read)
      {
        syslog(LOG_ERR, "Paritial Error %s\n\r", strerror(errno));
        continue;
      }
      close(fd);
      printf("wrote %d bytes\n\r", write_bytes);

      if(strchr(buffer, '\n'))
      {
        break;
      }
    }
    
    int read_offset = 0;
    // write back the data
    while(1)
    {
      int fd = open("/var/tmp/aesdsocketdata", O_RDWR | O_CREAT | O_APPEND, 0766);
      if(fd == -1)
      {
        syslog(LOG_ERR, "file open error: %s\n\r", strerror(errno));
        continue; 
      }
      lseek(fd, read_offset, SEEK_SET);
      int read_bytes = read(fd, buffer, bufferlen);

      close(fd);
      if(read_bytes == -1)
      {
        syslog(LOG_ERR, "Error reading: %s\n\r", strerror(errno));
        continue;
      }
      if(read_bytes == 0)
             break;
      // int write_bytes = write(client_sockfd, buffer, read_bytes);
      int write_bytes = send(client_sockfd, buffer, read_bytes,0);
      printf(" wrote %d bytes to client\n",write_bytes);

      if(write_bytes == -1)
      {
        syslog(LOG_ERR, "Error writing to client: %s\n\r", strerror(errno));
        continue;
      }
      // lseek(fd, write_bytes, SEEK_CUR);
      read_offset+=write_bytes;

    }

  close(client_sockfd);
  client_sockfd = -1;
  }
  close_connections();
  return 0;
}

