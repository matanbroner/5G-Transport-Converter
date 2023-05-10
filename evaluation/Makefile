# Makefile for MPTCP evaluation files

CC = gcc
CFLAGS = -Wall -Wextra -Werror -g -O2 -std=gnu99 -D_GNU_SOURCE

echo_client: 
	$(CC) $(CFLAGS) -o mptcp_echo_client mptcp_echo_client.c

downlink_client:
	$(CC) $(CFLAGS) -o mptcp_downlink_client mptcp_downlink_client.c

clean:
	rm -f *.o mptcp_echo_client mptcp_downlink_client