# Makefile for Transport Converter Library
# lib_convert depends on convert_util


# CC = gcc

# all: lib_sock.so lib_convert.so

# lib_sock.so: lib_sock.c map.h lib_convert/convert.h lib_convert/convert_util.h lib_convert/convert_util.c
# 	$(CC) -fPIC -shared -o lib_sock.so lib_sock.c -ldl

# convert_util.a: lib_convert/convert_util.c lib_convert/bsd_queue.h
# 	$(CC) -fPIC -c lib_convert/convert_util.c
# 	ar rcs convert_util.a convert_util.o 

# lib_convert.so: lib_convert/convert.h lib_convert/bsd_queue.h convert_util.a
# 	$(CC) -fPIC -shared -o lib_convert.so convert_util.a lib_convert/convert_client.c -ldl

# clean:
# 	rm -f *.o
# 	rm -f *.so
# 	rm -f *.a

CC = gcc
DEV_CFLAGS = -Wno-unused-variable -Wno-unused-function
CFLAGS = -fPIC -Wall -Wextra $(DEV_CFLAGS)
LDFLAGS = -shared

CLIENT_SRCS = lib_convert/convert_client.c lib_convert/convert_util.c
CLIENT_OBJS = $(CLIENT_SRCS:.c=.o)
CLIENT_TARGET = lib_convert.so

SERVER_SRCS = tc_server/tc_server.c lib_convert/convert_util.c 
SERVER_OBJS = $(SERVER_SRCS:.c=.o)
SERVER_TARGET = server

all: $(CLIENT_TARGET) $(SERVER_TARGET)

$(CLIENT_TARGET): $(CLIENT_OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

$(SERVER_TARGET): $(SERVER_OBJS)
	$(CC) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean
clean:
	rm -f *.o
	rm -f *.so
	rm -f *.a
	rm -f lib_convert/*.o
	rm -f lib_convert/*.so
	rm -f lib_convert/*.a
	rm -f tc_server/*.o
	rm -f tc_server/*.so
	rm -f tc_server/*.a
	rm -f server || true
