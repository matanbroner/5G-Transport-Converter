/**
 * Copyright(c) 2019, Tessares S.A.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and / or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 *        SERVICES; LOSS OF USE, DATA, OR PROFITS;
 *        OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

//  CONVERT_LOG=/tmp/converter.log CONVERT_ADDR=127.0.0.1 CONVERT_PORT=1234 

#define _GNU_SOURCE
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <syscall.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <stdarg.h>
#include <dlfcn.h>
#include <linux/mptcp.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include "convert.h"
#include "convert_util.h"
#include "bsd_queue.h"

#define CONVERT_PORT "1234"

enum
{
	SYSCALL_SKIP = 0,
	SYSCALL_RUN = 1,
};

/* State for each created TCP-based socket */
typedef struct socket_state
{
	LIST_ENTRY(socket_state)
	list;
	int fd;
	int established;
} socket_state_t;

#define NUM_BUCKETS 1024
static LIST_HEAD(socket_htbl_t, socket_state) _socket_htable[NUM_BUCKETS];
/* note: global hash table mutex, improvement: lock per bucket */
static pthread_mutex_t _socket_htable_mutex = PTHREAD_MUTEX_INITIALIZER;

static struct addrinfo *_converter_addr;
static const char *_convert_port = CONVERT_PORT;
static const char *_convert_cookie = NULL;

static FILE *_log;
static pthread_mutex_t _log_mutex = PTHREAD_MUTEX_INITIALIZER;

static int
_hash(int fd)
{
	return fd % NUM_BUCKETS;
}

static socket_state_t *
_lookup(int fd)
{
	int hash = _hash(fd);
	socket_state_t *state = NULL;
	socket_state_t *ret = NULL;

	pthread_mutex_lock(&_socket_htable_mutex);
	LIST_FOREACH(state, &_socket_htable[hash], list)
	{
		if (state->fd == fd)
		{
			ret = state;
			break;
		}
	}
	pthread_mutex_unlock(&_socket_htable_mutex);
	return ret;
}

static void
_alloc(int fd)
{
	socket_state_t *state;
	int hash = _hash(fd);

	state = (socket_state_t *)malloc(sizeof(*state));
	if (!state)
		return;

	printf("allocate state for fd %d\n", fd);

	state->fd = fd;

	pthread_mutex_lock(&_socket_htable_mutex);
	LIST_INSERT_HEAD(&_socket_htable[hash], state, list);
	pthread_mutex_unlock(&_socket_htable_mutex);
}

static void
_free_state(socket_state_t *state)
{
	pthread_mutex_lock(&_socket_htable_mutex);
	LIST_REMOVE(state, list);
	pthread_mutex_unlock(&_socket_htable_mutex);

	free(state);
}

static void
_free(int fd)
{
	socket_state_t *state;

	state = _lookup(fd);
	if (!state)
		/* no state associated with this fd. */
		return;

	printf("release state for fd %d\n", fd);

	_free_state(state);
}

static void
_set_no_linger(socket_state_t *state)
{
	struct linger linger = {1, 0};

	setsockopt(state->fd, SOL_SOCKET, SO_LINGER, &linger, sizeof(linger));
}

static void
_to_v4mapped(in_addr_t from, struct in6_addr *to)
{
	*to = (struct in6_addr){
		.s6_addr32 = {
			0,
			0,
			htonl(0xffff),
			from,
		},
	};
}

static ssize_t
_redirect_connect_tlv(uint8_t *buf, size_t buf_len, struct sockaddr *addr)
{
	ssize_t ret;
	struct convert_opts opts = {0};

	opts.flags = CONVERT_F_CONNECT;

	if (_convert_cookie)
	{
		opts.flags |= CONVERT_F_COOKIE;
		opts.cookie_len = strlen(_convert_cookie);
		opts.cookie_data = (uint8_t *)_convert_cookie;
	}

	switch (addr->sa_family)
	{
	case AF_INET:
	{
		struct sockaddr_in *in = (struct sockaddr_in *)addr;

		/* already in network bytes */
		_to_v4mapped(in->sin_addr.s_addr, &opts.remote_addr.sin6_addr);
		opts.remote_addr.sin6_port = in->sin_port;
		break;
	}
	case AF_INET6:
	{
		struct sockaddr_in6 *in6 = (struct sockaddr_in6 *)addr;

		/* already in network bytes */
		memcpy(&opts.remote_addr, in6, sizeof(opts.remote_addr));
		break;
	}
	default:
		return -EAFNOSUPPORT;
	}

	ret = convert_write(buf, buf_len, &opts);
	if (ret < 0)
	{
		printf("unable to allocate the convert header\n");
		ret = -EOPNOTSUPP;
	}
	return ret;
}

static int
_redirect(socket_state_t *state, struct sockaddr *dest)
{
	uint8_t buf[1024];
	ssize_t len;

	len = _redirect_connect_tlv(buf, sizeof(buf), dest);

	if (len < 0)
		return len;

	// use TCP fast open to send the convert header in the SYN packet
	return sendto(state->fd, buf, len, MSG_FASTOPEN, _converter_addr->ai_addr,
				  _converter_addr->ai_addrlen);
}

int socket(int domain, int type, int protocol)
{
	printf("socket intercept\n");
	/* Execute the socket() syscall to learn the file descriptor.
	 * Transform the domain to IPv4 or IPv6 depending on the
	 * Converter address family (address families of end-server
	 * and of Converter don't have to match).
	 */
	int (*lsocket)(int, int, int) = dlsym(RTLD_NEXT, "socket");
	int fd = lsocket(domain, type, protocol);
	/* Only consider IPv4/IPv6, SOCK_STREAM, IPPROTO_MPTCP based sockets. */
	if ((domain == AF_INET || domain == AF_INET6) && type == SOCK_STREAM && protocol == IPPROTO_MPTCP)
	{
		printf("socket(%d, %d, %d)\n", domain, type, protocol);
		printf("-> fd: %d\n", fd);

		if (fd >= 0)
			_alloc(fd);
	}
	return fd;
}

#define CONVERT_HDR_LEN sizeof(struct convert_header)

static int
_read_convert(socket_state_t *state, bool peek, int fail_errno)
{
	uint8_t hdr[CONVERT_HDR_LEN];
	int ret;
	int flag = peek ? MSG_PEEK : 0;
	size_t length;
	size_t offset = peek ? CONVERT_HDR_LEN : 0;
	struct convert_opts *opts;

	printf("peek fd %d to see whether data is in the receive "
		   "queue\n",
		   state->fd);

	ret = recvfrom(state->fd, hdr, CONVERT_HDR_LEN, MSG_WAITALL | flag, NULL, NULL);

	printf("peek returned %d\n", ret);
	if (ret < 0)
	{
		/* In case of error we want to skip the actual call.
		 * Moreover, if return EAGAIN (non-blocking) we don't
		 * want to app to receive the buffer.
		 */
		goto skip;
	}

	if (convert_parse_header(hdr, ret, &length) < 0)
	{
		printf("[%d] unable to read the convert header\n",
			   state->fd);
		goto error;
	}

	if (length)
	{
		uint8_t buffer[length + offset];

		/* if peek the data was not yet read, so we need to
		 * also read (again the main header). */
		if (peek)
			length += CONVERT_HDR_LEN;

		ret = recvfrom(state->fd, buffer, length, MSG_WAITALL, NULL, NULL);
		if (ret != (int)length || ret < 0)
		{
			printf("[%d] unable to read the convert"
				   " tlvs\n",
				   state->fd);
			goto error;
		}

		opts = convert_parse_tlvs(buffer + offset, length - offset);
		if (opts == NULL)
			goto error;

		/* if we receive the TLV error we need to inform the app */
		if (opts->flags & CONVERT_F_ERROR)
		{
			printf("received TLV error: %u\n", opts->error_code);
			goto error_and_free;
		}
	}

	/* everything was fine : free the state */
	if (!peek)
		_free_state(state);

	return SYSCALL_RUN;

error_and_free:
	convert_free_opts(opts);

error:
	printf("return error: -%d\n", fail_errno);

	/* ensure that a RST will be sent to the converter. */
	_set_no_linger(state);

	if (!peek)
		_free_state(state);

skip:
	return SYSCALL_SKIP;
}

int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
	int (*lconnect)(int, const struct sockaddr *, socklen_t) = dlsym(RTLD_NEXT, "connect");
	socket_state_t *state;
	int result;

	state = _lookup(sockfd);

	// if the socket is not managed by the library, just call the original
	// connect() syscall
	if (!state)
		return lconnect(sockfd, addr, addrlen);

	printf("redirecting fd %d\n", sockfd);

	switch (addr->sa_family)
	{
	case AF_INET:
	case AF_INET6:
		result = _redirect(state, (struct sockaddr *)addr);
		break;
	default:
		printf("fd %d specified an invalid address family %d\n", sockfd,
			   addr->sa_family);
		goto error;
	}

	if (result >= 0)
	{
		printf("redirection of fd %d in progress: %d\n", sockfd, result);

		/* If the sendto succeeded the received queue can be peeked to
		 * check
		 * the answer from the converter. */
		if (result > 0)
			_read_convert(state, false, ECONNREFUSED);

		return -ECONNREFUSED;
	}

	printf("the redirection of fd %d failed with error: %d\n", sockfd, result);

	/* If the redirection failed during the connect() there are no benefit to
	 * keep the allocated state. Clear it and behave like we never handled
	 * that file descriptor.
	 */
error:
	_free_state(state);
	return lconnect(sockfd, addr, addrlen);
}

int close(int sockfd)
{
	/* free any state created. */
	_free(sockfd);
	int (*lclose)(int) = dlsym(RTLD_NEXT, "close");

	return lclose(sockfd);
}

ssize_t
recv(int sockfd, void *buf, size_t len, int flags)
{
	socket_state_t *state;
	ssize_t (*lrecv)(int, void *, size_t, int) = dlsym(RTLD_NEXT, "recv");

	state = _lookup(sockfd);
	if (!state)
		return lrecv(sockfd, buf, len, flags);

	switch (_read_convert(state, false, ECONNREFUSED))
	{
	case SYSCALL_RUN:
		return lrecv(sockfd, buf, len, flags);
	case SYSCALL_SKIP:
		return 0;
	default:
		return -1;
	}
}

ssize_t
send(int sockfd, const void *buf, size_t len, int flags)
{
	socket_state_t *state;
	ssize_t (*lsend)(int, const void *, size_t, int) = dlsym(RTLD_NEXT, "send");

	state = _lookup(sockfd);
	if (!state)
		return lsend(sockfd, buf, len, flags);

	switch (_read_convert(state, false, ECONNREFUSED))
	{
	case SYSCALL_RUN:
		return lsend(sockfd, buf, len, flags);
	case SYSCALL_SKIP:
		return 0;
	default:
		return -1;
	}
}

static int
_read_sysctl_fastopen(unsigned int *flags)
{
	int fd;
	int rc;
	char buffer[1024];
	char *endp;

	fd = open("/proc/sys/net/ipv4/tcp_fastopen", O_RDONLY);
	if (fd < 0)
	{
		printf("unable to open /proc/sys/net/ipv4/tcp_fastopen: %s\n",
			   strerror(errno));
		return fd;
	}

	rc = read(fd, buffer, sizeof(buffer));
	close(fd);

	if (rc < 0)
	{
		printf("unable to read /proc/sys/net/ipv4/tcp_fastopen: %s\n",
			   strerror(errno));
		return rc;
	}

	/* contains a base 10 number */
	*flags = strtol(buffer, &endp, 10);

	if (*endp && *endp != '\n')
	{
		printf("unable to parse /proc/sys/net/ipv4/tcp_fastopen\n");
		return -1;
	}

	return 0;
}

static int
_validate_sysctl_fastopen()
{
	unsigned int flags = 0;
	unsigned int expected_flags = (0x1 | 0x4);

	if (_read_sysctl_fastopen(&flags) < 0)
		return -1;

	/* We use the fastopen backend without requiring exchange of actual
	 * fastopen options. expected_flags are: 0x1 (sending data in SYN) and
	 * 0x4 (regardless of cookie availability and without a cookie option).
	 */
	if ((flags & expected_flags) != expected_flags)
	{
		printf("the sysctl tcp_fastopen has an inappropriate value. Expect %x got %x.\n", expected_flags,
			   flags);
		return -1;
	}

	printf("fastopen sysctl is %x, expect flags %x to be set\n", flags,
		   expected_flags);

	return 0;
}

static void
_extract_kernel_version(const char *release, int *version, int *major)
{
	char *ptr;

	*version = strtol(release, &ptr, 10);
	ptr++; /* skip '.' */
	*major = strtol(ptr, NULL, 10);
}

static int
_validate_kernel_version()
{
	struct utsname kernel;
	int version, major;

	if (uname(&kernel) != 0)
	{
		printf("unable to retrieve the kernel version: %s\n",
			   strerror(errno));
		return -1;
	}

	printf("running kernel version: %s\n", kernel.release);

	_extract_kernel_version(kernel.release, &version, &major);
	printf("kernel base version: %d.%d\n", version, major);

	/* The TCP stack will acknowledge data sent in the SYN+ACK. */
	if (version > 4 || (version == 4 && major >= 5))
		return 0;

	printf("need at least kernel 4.5 to run this correctly\n");
	return -1;
}

static int
_validate_parameters()
{
	const char *convert_addr = getenv("CONVERT_ADDR");
	const char *convert_port = getenv("CONVERT_PORT");
	const char *convert_cookie = getenv("CONVERT_COOKIE");

	if (!convert_addr)
	{
		printf("environment variable 'CONVERT_ADDR' missing\n");
		return -1;
	}

	/* set port */
	if (convert_port)
	{
		char *endp;

		/* contains a base 10 number */
		strtol(convert_port, &endp, 10);

		if (*endp && *endp != '\n')
			printf(
				"unable to parse port: %s. Falling back to default port.\n",
				convert_port);
		else
			_convert_port = convert_port;
	}

	if (convert_cookie)
		_convert_cookie = convert_cookie;

	/* resolve address */
	if (getaddrinfo(convert_addr, _convert_port, NULL,
					&_converter_addr) != 0)
	{
		printf("unable to resolve '%s'\n", convert_addr);
		return -1;
	}

	printf("connecting to convert service at %s:%s%s%s\n",
		   convert_addr, _convert_port,
		   _convert_cookie ? " with cookie: " : "",
		   _convert_cookie ?: "");

	return 0;
}

static int
_validate_config()
{
	int ret = 0;

	ret = _validate_sysctl_fastopen();
	if (ret < 0)
		return ret;

	printf("sysctl tcp_fastopen is set correctly\n");

	ret = _validate_kernel_version();
	if (ret < 0)
		return ret;

	printf("kernel version is set correctly\n");

	return _validate_parameters();
}

static void
_log_lock(UNUSED void *udata, int lock)
{
	if (lock)
		pthread_mutex_lock(&_log_mutex);
	else
		pthread_mutex_unlock(&_log_mutex);
}

static __attribute__((constructor)) void
init(void)
{
	char err_buf[1024];
	const char *log_path = getenv("CONVERT_LOG");

	// log_set_quiet(1);
	// log_set_lock(_log_lock);

	/* open the log iff specified */
	// if (log_path)
	// {
	// 	_log = fopen(log_path, "w");
	// 	if (!_log)
	// 		fprintf(stderr, "convert: unable to open log %s: %s",
	// 				log_path, strerror(errno));
	// 	log_set_fp(_log);
	// }

	printf("Starting interception\n");

	if (_validate_config() < 0)
	{
		printf("init failed\n");
		printf("Unable to setup connection interception: %s.\n",
			   err_buf);
		exit(1);
	}
	printf("init done\n");
}

static __attribute__((destructor)) void
fini(void)
{
	printf("Terminating interception\n");

	freeaddrinfo(_converter_addr);

	if (_log)
		fclose(_log);
}