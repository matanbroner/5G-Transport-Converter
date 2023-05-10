# Makefile for MPTCP evaluation files

CC = gcc
CFLAGS = -Wall -Wextra -Werror -g -O2 -std=gnu99 -D_GNU_SOURCE

all: 
	$(CC) $(CFLAGS) -o mptcp_client mptcp_client.c

clean:
	rm -f *.o mptcp_client