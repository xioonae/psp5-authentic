/* psp5-authentic – the sockets of the FTP server */
#ifndef NETSOCK_H
#define NETSOCK_H

/* What the FTP server (ftp.c) needs from the network, nothing more. On the PSP it is the Inet library (netsock.c); the tests on a PC have their own.
 * Every socket is non-blocking: a call that has nothing to do returns at once. The network libraries must be started and connected (netlink.c). */

#define NETSOCK_AGAIN (-1) /* nothing to read / the other side is not ready: ask again next frame */
#define NETSOCK_ERR   (-2) /* the connection is broken */

/* A socket that listens on `port` (0 = any), or -1. */
int netsock_listen(unsigned short port);
/* A connection that was waiting on the listening socket `ls`, as a new socket, or -1 when none waits. */
int netsock_accept(int ls);
/* The port of a socket (a listening one: the port it got), 0 when unknown. */
unsigned short netsock_port(int s);
/* The address of this PSP on the connection `s`, as a.b.c.d = a << 24 | b << 16 | c << 8 | d, 0 when unknown. */
unsigned netsock_local_ip(int s);
/* Bytes that were read (up to `n`); 0 = the other side has closed; NETSOCK_AGAIN = nothing now; NETSOCK_ERR. */
int netsock_recv(int s, void *buf, int n);
/* Bytes that were taken (0..n, 0 = the buffers are full now); NETSOCK_ERR. */
int netsock_send(int s, const void *buf, int n);
void netsock_close(int s);

#endif
