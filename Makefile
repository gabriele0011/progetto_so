CC = gcc
CFLAGS = -Wall -pedantic


client: client.o aux_function.o list.o
	$(CC) $(CFLAGS) client.o aux_function.o list.o -o client
client.o: client.c client.h
aux_function.o: aux_function.c aux_function.h
list.o: list.c list.h


server: server_manager.o list.o cache.o conc_queue.o aux_function.o
	$(CC) $(CFLAGS) server_manager.o list.o cache.o conc_queue.o aux_function.o -o server
server_manager.o: server_manager.c server_manager.h
list.o: list.c list.h
cache.o: cache.c cache.h
conc_queue.o: conc_queue.c conc_queue.h
aux_function.o: aux_function.c aux_function.h



.PHONY : clean
clean:
	-rm mysock
