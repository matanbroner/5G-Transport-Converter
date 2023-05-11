# Makefile for Transport Converter

CC = gcc
DEV_CFLAGS = -Wno-unused-variable -Wno-unused-function
CFLAGS = -fPIC -Wall -Wextra $(DEV_CFLAGS)
LDFLAGS = -shared

CLIENT_SRCS = lib_convert/convert_client.c lib_convert/convert_util.c
CLIENT_OBJS = $(CLIENT_SRCS:.c=.o)
CLIENT_TARGET = lib_convert.so


all: $(CLIENT_TARGET) $(SERVER_TARGET)

$(CLIENT_TARGET): $(CLIENT_OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

setup_transport_converter: 
	python3 -m venv venv
	. venv/bin/activate
	pip install -r ./5GTC/requirements.txt
	pip install ./5GTC/pkg/mptcp_util

run_transport_converter:
	. venv/bin/activate
	cd 5GTC && sudo -E python3 main.py

.PHONY: clean
clean:
	rm -f *.o
	rm -f *.so
	rm -f *.a
	rm -f lib_convert/*.o
	rm -f lib_convert/*.so
	rm -f lib_convert/*.a
	rm -R -f venv