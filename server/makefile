CROSS_COMPILE = 

ifeq ($(CC),)
	CC = $(CROSS_COMPILE)gcc
endif

ifeq ($(CFLAGS),)
	CFLAGS = -Wall -Werror -g 
endif

ifeq ($(LDFLAGS),)
	LDFLAGS = -pthread -lrt
endif
	
all: aesdsocket
	
aesdsocket: aesdsocket.o
	$(CC) $(CFLAGS) $(LDFLAGS) aesdsocket.o -o aesdsocket
	
aesdsocket.o: aesdsocket.c
	$(CC) $(CFLAGS) $(LDFLAGS) -c aesdsocket.c

clean:
	rm -f *.o aesdsocket  
