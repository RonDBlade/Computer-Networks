COMP_FLAG = -g=99 -Wall -Wextra -Werror -pedantic-errors
OBJS = server_side.o client_side.o

seker_server: server_side.o
	gcc -o $@ server_side.o

server_side.o: server_side.c
	gcc $(COMP_FLAG) -c $*.c
	
seker_server: client_side.o
	gcc -o $@ client_side.o

client_side.o: client_side.c
	gcc $(COMP_FLAG) -c $*.c

clean:
	rm *~$(OBJS)