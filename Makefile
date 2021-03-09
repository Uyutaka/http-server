CC := gcc
CFLAGS=-std=c11


http-server: http-server.c

run :
	./http-server 7000

clean:
	rm -f http-server
.PHONY: clean run
