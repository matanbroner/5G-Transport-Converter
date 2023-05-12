# Makefile for Transport Converter

CC = gcc
DEV_CFLAGS = -Wno-unused-variable -Wno-unused-function
CFLAGS = -fPIC -Wall -Wextra $(DEV_CFLAGS)
LDFLAGS = -shared
ENV_NAME = venv

CLIENT_SRCS = lib_convert/convert_client.c lib_convert/convert_util.c
CLIENT_OBJS = $(CLIENT_SRCS:.c=.o)
CLIENT_TARGET = lib_convert.so


all: $(CLIENT_TARGET) $(SERVER_TARGET)

$(CLIENT_TARGET): $(CLIENT_OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

setup_transport_converter: 
	python3 -m venv $(ENV_NAME)
	$(ENV_NAME)/bin/pip install --upgrade pip
	$(ENV_NAME)/bin/pip install -r ./5GTC/requirements.txt
	$(ENV_NAME)/bin/pip install install ./5GTC/pkg/mptcp_util
	$(ENV_NAME)/bin/pip install install ./pyroute2

run_transport_converter:
	cd 5GTC && ../$(ENV_NAME)/bin/python3 ./main.py

test_transport_converter:
	cd 5GTC &&  sudo -E ../$(ENV_NAME)/bin/python3 -m unittest discover

.PHONY: clean
clean:
	rm -f *.o
	rm -f *.so
	rm -f *.a
	rm -f lib_convert/*.o
	rm -f lib_convert/*.so
	rm -f lib_convert/*.a
	rm -R -f venv