CROSS_COMPILE = 

CC = $(CROSS_COMPILE)gcc
CFLAGS = -Wall -Werror -g 
	
all: writer
	
writer: writer.o
	$(CC) $(CFLAGS) writer.o -o writer
	
writer.o: writer.c
	$(CC) $(CFLAGS) -c writer.c

clean:
	rm -f *.o writer  
