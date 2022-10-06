CROSS_COMPILE = 

CC = $(CROSS_COMPILE)gcc
CFLAGS = -Wall -Werror -g 
	
all: aesdsocket
	
aesdsocket: aesdsocket.o
	$(CC) $(CFLAGS) aesdsocket.o -o aesdsocket
	
aesdsocket.o: aesdsocket.c
	$(CC) $(CFLAGS) -c aesdsocket.c

clean:
	rm -f *.o aesdsocket  
