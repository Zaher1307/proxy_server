# Makefile for Proxy Lab 


CC = gcc
CFLAGS = -g -Wall
LDFLAGS = -lpthread

all: proxy

proxy.o: src/proxy.c 
	$(CC) $(CFLAGS) -c src/proxy.c

serve.o: src/proxy_serve/serve.c
	$(CC) $(CFLAGS) -c src/proxy_serve/serve.c

sio.o: src/safe_input_output/sio.c
	$(CC) $(CFLAGS) -c src/safe_input_output/sio.c

interface.o: src/socket_interface/interface.c
	$(CC) $(CFLAGS) -c src/socket_interface/interface.c

proxy: proxy.o serve.o sio.o interface.o
	$(CC) $(CFLAGS) proxy.o serve.o sio.o interface.o -o proxy $(LDFLAGS)

clean:
	rm -f *~ *.o proxy core *.tar *.zip *.gzip *.bzip *.gz

