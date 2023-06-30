all:
	make compile

clean:
	rm -rf ./client ./server

compile:
	gcc -Wall ./client.c -o ./client
	gcc -Wall ./server.c -o ./server