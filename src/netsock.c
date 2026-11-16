/* psp5-authentic – the sockets of the FTP server on the PSP (see netsock.h): the Inet library, every socket non-blocking.
 * The library is started by netlink.c (sceNetInit, sceNetInetInit). The functions are imported in netstub.S.
 * Before a socket is read, written or accepted on it is asked (sceNetInetSelect, no waiting) whether it can be: then no call can ever wait, whatever mode the socket is
 * in (PPSSPP, for one, makes the sockets that accept() returns blocking), and no error number has to be told apart from "not now". */
#include <pspnet_inet.h>
#include <netinet/in.h>
#include <string.h>
#include "netsock.h"

/* The error of a call that had nothing to do: the Inet library's errno 11 (EAGAIN = EWOULDBLOCK; the same number as in the SDK's headers), also written as the
 * system's error number (0x8001000B), and 35 (the BSD number). */
static int would_block(void)
{
    int e = sceNetInetGetErrno();
    return e == 11 || e == 35 || (unsigned)e == 0x8001000Bu;
}

/* The sets of sockets that sceNetInetSelect takes are 256 bits (32 bytes) on the PSP; the fd_set of the SDK's headers is smaller (64 bits), and the library would write
 * past its end. So the set is made here. */
typedef struct {
    unsigned int bits[8];
} NetFdSet;

/* 1 = `s` can be read (a connection waits, data has come, or the other side has closed) or, with `write`, written; 0 = not now; -1 = broken. */
static int ready(int s, int write)
{
    NetFdSet set;
    SceNetInetTimeval none = { 0, 0 };
    if (s < 0 || s >= 256)
        return -1;
    memset(&set, 0, sizeof set);
    set.bits[s / 32] |= 1u << (s % 32);
    return write ? sceNetInetSelect(s + 1, NULL, (fd_set *)&set, NULL, &none) : sceNetInetSelect(s + 1, (fd_set *)&set, NULL, NULL, &none);
}

static int nonblock(int s)
{
    int one = 1;
    return sceNetInetSetsockopt(s, SOL_SOCKET, SO_NONBLOCK, &one, sizeof one);
}

int netsock_listen(unsigned short port)
{
    int s = sceNetInetSocket(AF_INET, SOCK_STREAM, 0);
    if (s < 0)
        return -1;
    int one = 1;
    sceNetInetSetsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    struct sockaddr_in a;
    memset(&a, 0, sizeof a);
    a.sin_len = sizeof a;
    a.sin_family = AF_INET;
    a.sin_port = htons(port);
    if (sceNetInetBind(s, (struct sockaddr *)&a, sizeof a) < 0 || sceNetInetListen(s, 4) < 0 || nonblock(s) < 0) {
        sceNetInetClose(s);
        return -1;
    }
    return s;
}

int netsock_accept(int ls)
{
    struct sockaddr_in a;
    socklen_t l = sizeof a;
    memset(&a, 0, sizeof a);
    if (ready(ls, 0) <= 0)
        return -1;
    int c = sceNetInetAccept(ls, (struct sockaddr *)&a, &l);
    if (c < 0)
        return -1;
    if (nonblock(c) < 0) {
        sceNetInetClose(c);
        return -1;
    }
    return c;
}

unsigned short netsock_port(int s)
{
    struct sockaddr_in a;
    socklen_t l = sizeof a;
    memset(&a, 0, sizeof a);
    if (sceNetInetGetsockname(s, (struct sockaddr *)&a, &l) < 0)
        return 0;
    return ntohs(a.sin_port);
}

unsigned netsock_local_ip(int s)
{
    struct sockaddr_in a;
    socklen_t l = sizeof a;
    memset(&a, 0, sizeof a);
    if (sceNetInetGetsockname(s, (struct sockaddr *)&a, &l) < 0)
        return 0;
    unsigned ip = a.sin_addr.s_addr; /* (in network order: the first byte of the address is the lowest here) */
    return ((ip & 255) << 24) | (((ip >> 8) & 255) << 16) | (((ip >> 16) & 255) << 8) | (ip >> 24);
}

int netsock_recv(int s, void *buf, int n)
{
    int rdy = ready(s, 0);
    if (rdy < 0)
        return NETSOCK_ERR;
    if (rdy == 0)
        return NETSOCK_AGAIN;
    int r = (int)sceNetInetRecv(s, buf, n, 0);
    if (r >= 0)
        return r;
    return would_block() ? NETSOCK_AGAIN : NETSOCK_ERR;
}

int netsock_send(int s, const void *buf, int n)
{
    int rdy = ready(s, 1);
    if (rdy < 0)
        return NETSOCK_ERR;
    if (rdy == 0)
        return 0;
    int r = (int)sceNetInetSend(s, buf, n, 0);
    if (r >= 0)
        return r;
    return would_block() ? 0 : NETSOCK_ERR;
}

void netsock_close(int s)
{
    if (s >= 0)
        sceNetInetClose(s);
}
