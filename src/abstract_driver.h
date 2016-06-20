/*
 * Copyright (c) 2015, CESAR.
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license. See the LICENSE file for details.
 *
 */
#include <stdio.h>
#include <stddef.h>

#ifndef ARDUINO
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#endif

#ifndef __ABSTRACT_DRIVER_H__
#define __ABSTRACT_DRIVER_H__

// invalid socket fd
#define SOCKET_INVALID		-1

// operation status codes
#define SUCCESS		0
#define ERROR			-1

#ifdef __cplusplus
extern "C"{
#endif

#ifdef ARDUINO
#define	EPERM			1		/* Operation not permitted */
#define	EIO					5		/* I/O error */
#define	EBADF			9		/* Bad file number */
#define	EAGAIN			11	/* Try again */
#define	ENOMEM		12	/* Out of memory */
#define	EACCES		13	/* Permission denied */
#define	EFAULT			14	/* Bad address */
#define	EINVAL			22	/* Invalid argument */
#define	EMFILE			24	/* Too many open files */

#define	ECOMM						70	/* Communication error on send */
#define	EOVERFLOW			75	/* Value too large for defined data type */
#define	EUSERS					87	/* Too many users */
#define	EMSGSIZE				90	/* Message too long */
#define	ECONNRESET		104	/* Connection reset by peer */
#define	ETIMEDOUT				110	/* Connection timed out */
#define	ECONNREFUSED	111	/* Connection refused */
#define	EHOSTDOWN			112	/* Host is down */
#define	EALREADY				114	/* Operation already in progress */
#define	EINPROGRESS		115	/* Operation now in progress */

extern int errno;

/* Standard file descriptors.  */
#define	STDIN_FILENO	0	/* Standard input.  */
#define	STDOUT_FILENO	1	/* Standard output.  */
#define	STDERR_FILENO	2	/* Standard error output.  */
#endif

 /**
 * struct abstract_driver - driver abstraction for the physical layer
 * @name: driver name
 * @probe: function to initialize the driver
 * @remove: function to close the driver and free its resources
 * @socket: function to create a new socket FD
 * @listen: function to enable incoming connections
 * @accept: function that waits for a new connection and returns a 'pollable' FD
 * @connect: function to connect to a server socket
 *
 * This 'driver' intends to be an abstraction for Radio technologies or
 * proxy for other services using TCP or any socket based communication.
 */

typedef struct {
	const char *name;
	int (*probe)(size_t packet_size);
	void (*remove)(void);
	int (*socket)(void);
	int (*close)(int sockfd);
	int (*listen)(int sockfd, int backlog, const void *addr, size_t len);
	int (*connect)(int cli_sockfd, const void *addr, size_t len);
	int (*read)(int sockfd, void *buffer, size_t len);
	int (*write)(int sockfd, const void *buffer, size_t len);
} abstract_driver_t;

extern abstract_driver_t nrf24l01_driver;

#ifdef __cplusplus
} // extern "C"
#endif

#endif // __ABSTRACT_DRIVER_H__
