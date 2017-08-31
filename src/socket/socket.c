
/*
 * Copyright (c) 2014, Alper Akcan <alper.akcan@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *    * Neither the name of the <Alper Akcan> nor the
 *      names of its contributors may be used to endorse or promote products
 *      derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#if defined(__MINGW32__)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#endif
#include <fcntl.h>
#include <errno.h>

#define MBUS_DEBUG_NAME	"mbus-socket"

#include "mbus/debug.h"
#include "socket.h"

struct mbus_socket {
	int domain;
	int type;
	int protocol;
	int fd;
};

static int mbus_socket_domain_to_posix (enum mbus_socket_domain domain)
{
	switch (domain) {
		case mbus_socket_domain_af_inet:		return AF_INET;
		case mbus_socket_domain_af_unix:		return AF_UNIX;
	}
	return -1;
}

static int mbus_socket_type_to_posix (enum mbus_socket_type type)
{
	switch (type) {
		case mbus_socket_type_sock_stream:		return SOCK_STREAM;
	}
	return -1;
}

static int mbus_socket_protocol_to_posix (enum mbus_socket_protocol protocol)
{
	switch (protocol) {
		case mbus_socket_protocol_any:		return 0;
	}
	return -1;
}

struct mbus_socket * mbus_socket_create (enum mbus_socket_domain domain, enum mbus_socket_type type, enum mbus_socket_protocol protocol)
{
	struct mbus_socket *s;
	s = malloc(sizeof(struct mbus_socket));
	if (s == NULL) {
		mbus_errorf("can not allocate memory");
		goto bail;
	}
	memset(s, 0, sizeof(struct mbus_socket));
	s->domain = mbus_socket_domain_to_posix(domain);
	if (s->domain < 0) {
		mbus_errorf("invalid domain");
		goto bail;
	}
	s->type = mbus_socket_type_to_posix(type);
	if (s->type < 0) {
		mbus_errorf("invalid type");
		goto bail;
	}
	s->protocol = mbus_socket_protocol_to_posix(protocol);
	if (s->protocol < 0) {
		mbus_errorf("invalid protocol");
		goto bail;
	}
	s->fd = socket(s->domain, s->type, s->protocol);
	if (s->fd < 0) {
		mbus_errorf("can not open socket");
		goto bail;
	}
	return s;
bail:	mbus_socket_destroy(s);
	return NULL;
}

void mbus_socket_destroy (struct mbus_socket *socket)
{
	if (socket == NULL) {
		return;
	}
	if (socket->fd >= 0) {
		close(socket->fd);
	}
	free(socket);
}

void mbus_socket_close (struct mbus_socket *socket)
{
	if (socket == NULL) {
		return;
	}
	if (socket->fd >= 0) {
		close(socket->fd);
		socket->fd = -1;
	}
}

int mbus_socket_get_fd (struct mbus_socket *socket)
{
	if (socket == NULL) {
		return -1;
	}
	return socket->fd;
}

int mbus_socket_set_reuseaddr (struct mbus_socket *socket, int on)
{
	int rc;
	int opt;
	if (socket == NULL) {
		mbus_errorf("socket is null");
		return -1;
	}
	opt = !!on;
	rc = setsockopt(socket->fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	if (rc < 0) {
		mbus_errorf("setsockopt reuseaddr failed");
		return -1;
	}
	return 0;
}

int mbus_socket_get_reuseaddr (struct mbus_socket *socket)
{
	int rc;
	int opt;
	socklen_t optlen;
	if (socket == NULL) {
		mbus_errorf("socket is null");
		return -1;
	}
	optlen = sizeof(opt);
	rc = getsockopt(socket->fd, SOL_SOCKET, SO_REUSEADDR, &opt, &optlen);
	if (rc < 0) {
		mbus_errorf("setsockopt reuseaddr failed");
		return -1;
	}
	return !!opt;
}

int mbus_socket_set_blocking (struct mbus_socket *socket, int on)
{
	int rc;
	int flags;
	if (socket == NULL) {
		mbus_errorf("socket is null");
		return -1;
	}
#if defined(__MINGW32__)
	u_long opt = on ? 1 : 0;
	rc = ioctlsocket(socket->fd, FIONBIO, &opt);
	if (rc != NO_ERROR) {
		return -1;
	}
#else
	flags = fcntl(socket->fd, F_GETFL, 0);
	if (flags < 0) {
		mbus_errorf("can not get flags");
		return -1;
	}
	flags = on ? (flags & ~O_NONBLOCK) : (flags | O_NONBLOCK);
	rc = fcntl(socket->fd, F_SETFL, flags);
	if (rc != 0) {
		mbus_errorf("can not set flags");
		return -1;
	}
#endif
	return 0;
}

int mbus_socket_get_blocking (struct mbus_socket *socket)
{
#if defined(__MINGW32__)
#warning "mbus_socket_get_blocking() for MinGW missing"
	return -1;
#else
	int flags;
	if (socket == NULL) {
		mbus_errorf("socket is null");
		return -1;
	}
	flags = fcntl(socket->fd, F_GETFL, 0);
	if (flags < 0) {
		mbus_errorf("can not get flags");
		return -1;
	}
	return flags & O_NONBLOCK;
#endif
}

int mbus_socket_set_keepalive (struct mbus_socket *socket, int on)
{
	int rc;
	int opt;
	if (socket == NULL) {
		mbus_errorf("socket is null");
		return -1;
	}
	opt = !!on;
	rc = setsockopt(socket->fd, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(opt));
	if (rc < 0) {
		mbus_errorf("setsockopt keepalive failed");
		return -1;
	}
	return 0;
}

int mbus_socket_get_keepalive (struct mbus_socket *socket)
{
	int rc;
	int opt;
	socklen_t optlen;
	if (socket == NULL) {
		mbus_errorf("socket is null");
		return -1;
	}
	optlen = sizeof(opt);
	rc = getsockopt(socket->fd, SOL_SOCKET, SO_KEEPALIVE, &opt, &optlen);
	if (rc < 0) {
		mbus_errorf("setsockopt keepalive failed");
		return -1;
	}
	return opt;
}

int mbus_socket_set_keepcnt (struct mbus_socket *socket, int value)
{
	int rc;
	int opt;
	if (socket == NULL) {
		mbus_errorf("socket is null");
		return -1;
	}
	opt = value;
#if defined(__MINGW32__)
#warning "mbus_socket_set_keepcnt() for MinGW missing"
#else
	rc = setsockopt(socket->fd, SOL_SOCKET, TCP_KEEPCNT, &opt, sizeof(opt));
	if (rc < 0) {
		mbus_errorf("setsockopt keepcnt failed");
		return -1;
	}
#endif	
	return 0;
}

int mbus_socket_get_keepcnt (struct mbus_socket *socket)
{
	int rc;
	int opt;
	socklen_t optlen;
	if (socket == NULL) {
		mbus_errorf("socket is null");
		return -1;
	}
	optlen = sizeof(opt);
#if defined(__MINGW32__)
#warning "mbus_socket_get_keepcnt() for MinGW missing"
#else
	rc = getsockopt(socket->fd, SOL_SOCKET, TCP_KEEPCNT, &opt, &optlen);
	if (rc < 0) {
		mbus_errorf("setsockopt keepcnt failed");
		return -1;
	}
#endif
	return opt;
}

int mbus_socket_set_keepidle (struct mbus_socket *socket, int value)
{
#if defined(TCP_KEEPIDLE)
	int rc;
	int opt;
	if (socket == NULL) {
		mbus_errorf("socket is null");
		return -1;
	}
	opt = value;
	rc = setsockopt(socket->fd, SOL_SOCKET, TCP_KEEPIDLE, &opt, sizeof(opt));
	if (rc < 0) {
		mbus_errorf("setsockopt keepidle failed");
		return -1;
	}
	return 0;
#else
	if (socket == NULL) {
		mbus_errorf("socket is null");
		return -1;
	}
	(void) value;
	mbus_errorf("setsockopt keepidle is not supported");
	return -1;
#endif
}

int mbus_socket_get_keepidle (struct mbus_socket *socket)
{
#if defined(TCP_KEEPIDLE)
	int rc;
	int opt;
	socklen_t optlen;
	if (socket == NULL) {
		mbus_errorf("socket is null");
		return -1;
	}
	optlen = sizeof(opt);
	rc = getsockopt(socket->fd, SOL_SOCKET, TCP_KEEPIDLE, &opt, &optlen);
	if (rc < 0) {
		mbus_errorf("setsockopt keepidle failed");
		return -1;
	}
	return opt;
#else
	if (socket == NULL) {
		mbus_errorf("socket is null");
		return -1;
	}
	mbus_errorf("setsockopt keepidle is not supported");
	return -1;
#endif
}

int mbus_socket_set_keepintvl (struct mbus_socket *socket, int value)
{
	int rc;
	int opt;
	if (socket == NULL) {
		mbus_errorf("socket is null");
		return -1;
	}
	opt = value;
#if defined(__MINGW32__)
#warning "mbus_socket_set_keepintvl() for MinGW missing"
#else
	rc = setsockopt(socket->fd, SOL_SOCKET, TCP_KEEPINTVL, &opt, sizeof(opt));
	if (rc < 0) {
		mbus_errorf("setsockopt keepintvl failed");
		return -1;
	}
#endif
	return 0;
}

int mbus_socket_get_keepintvl (struct mbus_socket *socket)
{
	int rc;
	int opt;
	socklen_t optlen;
	if (socket == NULL) {
		mbus_errorf("socket is null");
		return -1;
	}
	optlen = sizeof(opt);
#if defined(__MINGW32__)
#warning "mbus_socket_get_keepintvl() for MinGW missing"
#else
	rc = getsockopt(socket->fd, SOL_SOCKET, TCP_KEEPINTVL, &opt, &optlen);
	if (rc < 0) {
		mbus_errorf("setsockopt keepintvl failed");
		return -1;
	}
#endif
	return opt;
}

int mbus_socket_connect (struct mbus_socket *socket, const char *address, unsigned short port)
{
	int rc;
	struct sockaddr_in sockaddr_in;
#if defined(__MINGW32__)
#warning "mbus_socket_connect() MinGW does not support AF_UNIX"
#else
struct sockaddr_un sockaddr_un;
#endif
	if (address == NULL) {
		mbus_errorf("address is null");
		return -1;
	}
	if (socket->domain == AF_INET) {
		sockaddr_in.sin_family = socket->domain;
		rc = inet_pton(socket->domain, address, &sockaddr_in.sin_addr);
		if (rc <= 0) {
			mbus_errorf("inet_pton failed for: '%s'", address);
			goto bail;
		}
		sockaddr_in.sin_port = htons(port);
		rc = connect(socket->fd, (struct sockaddr *) &sockaddr_in , sizeof(sockaddr_in));
#if !defined(__MINGW32__)
	} else if (socket->domain == AF_UNIX) {
		sockaddr_un.sun_family = socket->domain;
		snprintf(sockaddr_un.sun_path, sizeof(sockaddr_un.sun_path) - 1, "%s:%d", address, port);
		rc = connect(socket->fd, (struct sockaddr *) &sockaddr_un , sizeof(sockaddr_un));
#endif
	} else {
		mbus_errorf("unknown socket domain");
		goto bail;
	}
	if (rc < 0) {
		mbus_errorf("connect failed");
		goto bail;
	}
	return 0;
bail:	return -1;
}

int mbus_socket_bind (struct mbus_socket *socket, const char *address, unsigned short port)
{
	int rc;
	struct sockaddr_in sockaddr_in;
#if defined(__MINGW32__)
#warning "mbus_socket_bind() MinGW does not support AF_UNIX"
#else
	struct sockaddr_un sockaddr_un;
#endif
	if (socket->domain == AF_INET) {
		sockaddr_in.sin_family = socket->domain;
		if (address == NULL) {
			sockaddr_in.sin_addr.s_addr = INADDR_ANY;
		} else {
			rc = inet_pton(socket->domain, address, &sockaddr_in.sin_addr);
			if (rc <= 0) {
				mbus_errorf("inet_pton failed for: '%s'", address);
				goto bail;
			}
		}
		sockaddr_in.sin_port = htons(port);
		rc = bind(socket->fd, (struct sockaddr *) &sockaddr_in , sizeof(sockaddr_in));
#if !defined(__MINGW32__)
	} else if (socket->domain == AF_UNIX) {
		sockaddr_un.sun_family = socket->domain;
		snprintf(sockaddr_un.sun_path, sizeof(sockaddr_un.sun_path) - 1, "%s:%d", address, port);
		if (mbus_socket_get_reuseaddr(socket) == 1) {
			unlink(sockaddr_un.sun_path);
		}
		rc = bind(socket->fd, (struct sockaddr *) &sockaddr_un , sizeof(sockaddr_un));
#endif
	} else {
		mbus_errorf("unknown socket domain");
		goto bail;
	}
	if (rc < 0) {
		mbus_errorf("bind failed (%s)", strerror(errno));
		goto bail;
	}
	return 0;
bail:	return -1;
}

int mbus_socket_listen (struct mbus_socket *socket, int backlog)
{
	int rc;
	if (socket == NULL) {
		mbus_errorf("socket is null");
		return -1;
	}
	rc = listen(socket->fd, backlog);
	if (rc < 0) {
		mbus_errorf("listen failed");
		return -1;
	}
	return 0;
}

struct mbus_socket * mbus_socket_accept (struct mbus_socket *socket)
{
	struct mbus_socket *s;
	struct sockaddr_in sockaddr_in;
	socklen_t socklen;
	s = malloc(sizeof(struct mbus_socket));
	if (s == NULL) {
		mbus_errorf("can not allocate memory");
		goto bail;
	}
	memset(s, 0, sizeof(struct mbus_socket));
	s->domain = socket->domain;
	s->type = socket->type;
	socklen = sizeof(struct sockaddr_in);
	s->fd = accept(socket->fd, (struct sockaddr *) &sockaddr_in, &socklen);
	if (s->fd < 0) {
		mbus_errorf("can not accept socket");
		goto bail;
	}
	return s;
bail:	mbus_socket_destroy(s);
	return NULL;
}

char * mbus_socket_fd_get_address (int fd, char *buffer, int length)
{
	struct sockaddr_storage addr;
	socklen_t addrlen;
	addrlen = sizeof(addr);
	if (getpeername(fd, (struct sockaddr *) &addr, &addrlen) == 0) {
		if (addr.ss_family == AF_INET) {
			if (inet_ntop(AF_INET, &((struct sockaddr_in *) &addr)->sin_addr.s_addr, buffer, length)) {
				return buffer;
			}
		} else if (addr.ss_family == AF_INET6) {
			if (inet_ntop(AF_INET6, &((struct sockaddr_in6 *) &addr)->sin6_addr.s6_addr, buffer, length)) {
				return buffer;
			}
		}
	}
	return NULL;
}

char * mbus_socket_get_address (struct mbus_socket *socket, char *buffer, int length)
{
	return mbus_socket_fd_get_address(socket->fd, buffer, length);
}

int mbus_socket_read (struct mbus_socket *socket, void *vptr, int n)
{
	int nleft;
	int nread;
	char *ptr;
	ptr = vptr;
	nleft = n;
	while (nleft > 0){
		if ((nread = read(socket->fd, ptr, nleft)) < 0 ){
			if (errno == EINTR) {
				nread = 0;
			} else if (errno == EAGAIN) {
				break;
			} else if (errno == EWOULDBLOCK) {
				break;
			} else {
				return -1;
			}
		} else if (nread == 0) {
			/* EOF */
			break;
		}
		nleft -= nread;
		ptr   += nread;
	}
	return n - nleft;
}

int mbus_socket_write (struct mbus_socket *socket, const void *vptr, int n)
{
	int nleft;
	int nwritten;
	const char *ptr;
	ptr = vptr;
	nleft = n;
	while (nleft > 0) {
		if ((nwritten = write(socket->fd, ptr, nleft)) <= 0) {
			if (errno == EINTR) {
				nwritten = 0;
			} else if (errno == EAGAIN) {
				break;
			} else if (errno == EWOULDBLOCK) {
				break;
			} else {
				return -1;
			}
		}
		nleft -= nwritten;
		ptr   += nwritten;
	}
	return n - nleft;
}
