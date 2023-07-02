CC=gcc 
CFLAGS=-Wall -Wextra -g 
THREADFLAG=-pthread
EXEC_USER=./user 
EXEC_SERVER=./server

all: $(EXEC_USER) $(EXEC_SERVER)

$(EXEC_USER): user.c
	$(CC) $(CFLAGS) $(THREADFLAG) user.c -o $(EXEC_USER)

$(EXEC_SERVER): server.c 
	$(CC) $(CFLAGS) $(THREADFLAG) server.c  -o $(EXEC_SERVER)

clean:
	rm -rf *.o server user