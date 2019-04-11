COMP_FLAG = -std=gnu99 -Wall -Wextra -Werror -pedantic-errors
OBJS = server_side.o client_side.o

all: seker_server seker_client

seker_server: server_side.o
	gcc -o $@ server_side.o

server_side.o: server_side.c
	gcc $(COMP_FLAG) -c $*.c
	
seker_client: client_side.o
	gcc -o $@ client_side.o

client_side.o: client_side.c
	gcc $(COMP_FLAG) -c $*.c

clean:
	rm *~$(OBJS)