/*
 * @filename: aesdsocket.c
 * @author: Ganesh KM
 * @Description: To build a native server socket for A6-part1 as per assignment instructions
 * @references: https://qemu.readthedocs.io/en/latest/system/devices/net.html
 *              https://beej.us/guide/bgnet/html/#what-is-a-socket 
 */

#include <stdio.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <syslog.h>
#include <errno.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <pthread.h>
#include <stdarg.h>
#include <sys/queue.h>
#include <time.h>
#include <sys/time.h>

#define PORT "9000"
#define OUTPUT_PATH "/var/tmp/aesdsocketdata"
#define buffer_len 2048
#define BACKLOG 10      //pending connections on the listen syscall


typedef struct 
{
    int client_fd;
    pthread_t thread;
    pthread_mutex_t* mutex;
    bool thread_complete_status;
} thread_data; 

struct slist_data_s
{
    thread_data   params;
    SLIST_ENTRY(slist_data_s) entries;
};

typedef struct slist_data_s slist_data_t;

//globals
static int sock_fd = -1;
//static int client_fd = -1;
struct addrinfo hints, *server_info;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
bool graceful_exit_flag = false;
static char buffer[buffer_len];

//Handling timestamps for SIGALRM
void print_timestamp()
{
   time_t ct;
   time(&ct);
   char timestamp_buffer[100] = {0};
   
   strftime(timestamp_buffer, sizeof(timestamp_buffer), "timestamp: %a, %d %b %Y %T %z\r\n", localtime(&ct));
   syslog(LOG_DEBUG,"timestamp: %s",timestamp_buffer);


   int fd = open(OUTPUT_PATH, O_RDWR | O_APPEND, 0644);
    if (fd == -1)
    {
	    syslog(LOG_ERR, "failed to open a file:%d\n", errno);
    }

   lseek(fd, 0, SEEK_END);

   if(pthread_mutex_lock(&lock)!= 0)
    {
        syslog(LOG_ERR, "Error in locking the mutex");
        close(fd);
    }

   int length = strlen(timestamp_buffer);

   int write_bytes = write(fd, timestamp_buffer, length );
         if(write_bytes != length)
            syslog(LOG_DEBUG, "unsuccessful write \n");
   syslog(LOG_DEBUG, "append time \n");

   if(pthread_mutex_unlock(&lock)!= 0)
    {
        syslog(LOG_ERR, "Error in locking the mutex");
        close(fd);
    }
//    close(fd);
}

void graceful_exit()
{
    if(sock_fd > -1)
    {
        shutdown(sock_fd, SHUT_RDWR);
        close(sock_fd);
    }
    pthread_mutex_destroy(&lock);
    closelog();
}


static void signal_handler(int sig)
{
    syslog(LOG_INFO, "Signal Caught %d\n\r", sig);
    
    if((sig == SIGINT) || (sig == SIGTERM))
    {
        graceful_exit_flag = true;
        graceful_exit();
    }
    if(sig == SIGALRM)
   {
      syslog(LOG_DEBUG,"caught alrm %d %d \n", sig, SIGALRM);
      print_timestamp();
      alarm(10);
      syslog(LOG_INFO, "alarm set to 10\n");
   }
}


void* thread_func(void* thread_params)
{  

    thread_data* data = (thread_data*)thread_params;

    int read_bytes = 0;

    while(1)
    {
	    read_bytes = read(data->client_fd, buffer, (buffer_len));
	    if (read_bytes == -1) 
	    {
	        syslog(LOG_ERR, "receive error \n");
            data->thread_complete_status = true;
            pthread_exit(NULL);
	    }

	    if (read_bytes == 0)
	    {
	        continue;
	    }

	    if (strchr(buffer, '\n')) 
	    {  
	        break; 
	    } 
    }

    int fd = open(OUTPUT_PATH, O_RDWR | O_APPEND, 0644);
    if (fd ==-1)
    {
	    syslog(LOG_ERR, "failed to open a file:%d\n", errno);
    }

    lseek(fd, 0, SEEK_END);
	
    if(pthread_mutex_lock(data->mutex)!=0)
    {
	    syslog(LOG_ERR, "Error in locking the mutex\n\r");
        data->thread_complete_status = true;
        pthread_exit(NULL);
    }

    int file_write = write(fd, buffer, read_bytes);
    if(file_write == -1)
    {
        syslog(LOG_ERR, "error while writing %d\n\r", errno);
        data->thread_complete_status = true;
        close(fd);
        pthread_exit(NULL);
    }

    lseek(fd, 0, SEEK_SET); 

    if(pthread_mutex_unlock(data->mutex)!=0)
    {
        syslog(LOG_ERR, "Error in unlocking the mutex\n\r");
        data->thread_complete_status = true;
        pthread_exit(NULL);
    }

    close(fd);

    //clearing the buffer
    memset(buffer, 0, buffer_len);
    int read_offset = 0;

    while(1) 
    {

        int fd = open(OUTPUT_PATH, O_RDWR | O_APPEND, 0644);
        if(fd == -1)
        {
            syslog(LOG_ERR, "failed to open a file:%d\n", errno);
            continue; 
        }

        lseek(fd, read_offset, SEEK_SET);

        if(pthread_mutex_lock(data->mutex)!=0)
        {
            syslog(LOG_ERR, "Error in locking the mutex\n\r");
            data->thread_complete_status = true;
            pthread_exit(NULL);
        }

        int read_bytes = read(fd, buffer, buffer_len);

        if(pthread_mutex_unlock(data->mutex)!=0)   
        {
            syslog(LOG_ERR, "Error in locking the mutex\n\r");
            data->thread_complete_status = true;
            pthread_exit(NULL);
        }

        close(fd);

        if(read_bytes == -1)
        {
            syslog(LOG_ERR, "failed to read from file:%d\n", errno);
            continue;
        }

        if(read_bytes == 0)
        {
            break;
        }

        int file_write = write(data->client_fd, buffer, read_bytes);

        if(file_write == -1)
        {
            syslog(LOG_ERR, "failed to write on client fd:%d\n", errno);
            continue;
        }

        read_offset += file_write;
    }

    data->thread_complete_status = true;
    pthread_exit(NULL);
}

int main(int argc, char **argv) 
{

    openlog("aesdsocket", 0, LOG_USER);
    //flags
    bool start_alrm=false;
    bool daemon_flag = false; 
    // register signals 
    if(signal(SIGINT, signal_handler)== SIG_ERR)   
    {
        syslog(LOG_ERR, "Error while registering SIGINT\n\r");
	    graceful_exit();
    }

    if(signal(SIGTERM, signal_handler) == SIG_ERR)
    {
	    syslog(LOG_ERR, "Error while registering SIGTERM\n\r");
	    graceful_exit();
    }
    //timestamp signal
    if(signal(SIGALRM, signal_handler) == SIG_ERR) 
    {
	    syslog(LOG_ERR, "Error while registering SIGTERM\n\r");
	    graceful_exit();
    }
    
    //check for daemon
    if (argc == 2) 
    {
        if (!strcmp(argv[1], "-d")) 
        {
            daemon_flag = true;
        } 
        else 
        {
            printf("wrong arg: %s\nUse -d option for running as daemon", argv[1]);
            syslog(LOG_ERR, "wrong arg: %s\nUse -d option for running as daemon", argv[1]);
            exit(EXIT_FAILURE);
        }
    }	
   
    int write_fd = creat(OUTPUT_PATH, 0766);
    if(write_fd < 0)
    {
	    syslog(LOG_ERR, "aesdsocketdata file could not be created, error number %d", errno);
	    graceful_exit();
	    exit(1);
    }
    close(write_fd);

    //Initialize the linked list
    slist_data_t *listPtr = NULL;

    SLIST_HEAD(slisthead, slist_data_s) head;
    SLIST_INIT(&head);

    memset(&hints, 0, sizeof(hints));

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;	
	
    int status;

    //int getaddrinfo(const char *node, const char *service, const struct addrinfo *hints, struct addrinfo **res)                 
    if((status = getaddrinfo(NULL, (PORT), &hints, &server_info))!=0)
    {
        syslog(LOG_ERR, "Error while registering SIGINT %s\n\r", strerror(errno));
        exit(1);
    }

    //create socket connection
   //int socket(int domain, int type, int protocol)
    if((sock_fd = socket(server_info->ai_family, server_info->ai_socktype, 0))< 0)
    {
        printf("socket creation failed \n");
        syslog(LOG_ERR, "socket creation failed %s\n\r", strerror(errno));
        exit(1);
    }

    // Handling the resue of port
    int yes = 1;
    if(setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0)
    {
        printf("setsockopt error\n");
        syslog(LOG_ERR, "setsockopt %s\n\r", strerror(errno));
        exit(1);
    }
  
    // bind system call
    //int bind(int sockfd, struct sockaddr *my_addr, int addrlen)
    if(bind(sock_fd, server_info->ai_addr, server_info->ai_addrlen) < 0)
    {
        printf("Bind error \n");
        syslog(LOG_ERR, "Bind error: %s\n\r", strerror(errno));
        exit(1);
    }

    freeaddrinfo(server_info);

    // Listen for connection
    //int listen(int sockfd, int backlog)
    if(listen(sock_fd, BACKLOG)== -1)
    {
        printf ("Listen error \n");
        syslog(LOG_ERR, "error on syscall Listen : %s\n\r", strerror(errno));
        exit(1);
    }

    // run as daemon
    if (daemon_flag == true) 
    {
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
    }
	

    while(!(graceful_exit_flag))
    {
	struct sockaddr_in client_addr;
	socklen_t client_addr_size = sizeof(client_addr);

	int client_fd = accept(sock_fd, (struct sockaddr*)&client_addr, &client_addr_size);
        
    if(client_fd < 0)
	{
	    syslog(LOG_ERR, "accepting new connection error is %s", strerror(errno));
	    graceful_exit();
	    exit(EXIT_FAILURE);
	} 
	   
	if(graceful_exit_flag)
	{
            break;
	}
       
	   
    char client_ip[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);    
	syslog(LOG_INFO, "Accepted connection from %s \n\r",client_ip);
	printf("Accepted connection from %s\n\r", client_ip);
	   
	listPtr = (slist_data_t *) malloc(sizeof(slist_data_t));

        SLIST_INSERT_HEAD(&head, listPtr, entries);

        listPtr->params.client_fd              = client_fd;
        listPtr->params.mutex                  = &lock;
        listPtr->params.thread_complete_status = false;

        pthread_create(&(listPtr->params.thread), NULL, thread_func, (void*)&listPtr->params);
        if(!start_alrm)
        {
            start_alrm=true;
            syslog(LOG_DEBUG,"starting alarm\n");
            alarm(10);
        }

        SLIST_FOREACH(listPtr,&head,entries)
        {     
            if(listPtr->params.thread_complete_status == true)
            {
                pthread_join(listPtr->params.thread,NULL);

                shutdown(listPtr->params.client_fd, SHUT_RDWR);

                close(listPtr->params.client_fd);

                syslog(LOG_INFO, "Join spawned thread:%d\n\r",(int)listPtr->params.thread); 
            }
        }
    }

    while (!SLIST_EMPTY(&head))
    {
        listPtr = SLIST_FIRST(&head);
        pthread_cancel(listPtr->params.thread);
        syslog(LOG_INFO, "Thread is killed:%d\n\r", (int) listPtr->params.thread);
        SLIST_REMOVE_HEAD(&head, entries);
        free(listPtr); 
        listPtr = NULL;
    }
	// if file can be accessed
    if (access(OUTPUT_PATH, F_OK) == 0) 
    {
	    remove(OUTPUT_PATH);
    }
	
    graceful_exit();
	
    exit(0);
	
}


