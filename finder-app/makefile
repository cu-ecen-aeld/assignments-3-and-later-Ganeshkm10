.DEFAULT_GOAL := writer

writer: writer.o
	$(CROSS_COMPILE)-gcc writer.o -o writer

writer.o:
	$(CROSS_COMPILE)-gcc -c writer.c

clean:
	rm -f *.o writer  
