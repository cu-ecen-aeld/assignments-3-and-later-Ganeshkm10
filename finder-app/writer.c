#include<stdio.h>
#include<errno.h>
#include<syslog.h>
#include<fcntl.h>
#include<string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

int main(int argc,char *argv[])
{

  openlog(NULL, 0, LOG_USER);
  
  int fd;

  if(argc != 3)
  {
   syslog(LOG_ERR, "Invalid Number of Arguments: %d", argc);
   printf(" Invalid Number of Arguments \n");
   printf(" Provide 2 arguments \n");
   printf(" the first argument is a path to a directory on the filesystem \n");
   printf(" second argument is a text string which will be searched within these files \n");
   return 1;
   
  }

  fd = open( argv[1] , O_RDWR, 0644);
  
  if(fd == -1)
  {
   syslog(LOG_ERR, "Error opening file %s: %s \n",argv[1], strerror(errno));
   printf(" File directory is not found \n");
   return 1;
  }
  
  int writelen = write( fd, argv[2] , strlen(argv[2]));
  if(writelen == -1)
  {
    syslog(LOG_ERR, "Error writing into file %s : %s \n",argv[1], strerror(errno));
    printf(" error while writing , errno = %d  : %s \n",errno, strerror(errno)); 
  }
  else if(writelen != strlen(argv[2]))
  {
    syslog(LOG_ERR, "partial write : %s \n",argv[1]);
    printf(" Short write occured of %d bytes instead of actual %ld bytes \n",writelen, strlen(argv[2])); 
  }
  else
  {
    syslog(LOG_DEBUG, "writting %s to %s", argv[2], argv[1]);
    printf(" writing %d bytes to %s \n", writelen, argv[1]); 
  }
  
  closelog();
  return 0;
  
}
