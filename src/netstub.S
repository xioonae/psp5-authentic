/* psp5 – made by Mille Weber */
/* The calls of the PSP's Inet library that psp5 uses, imported by hand: sceNetInetInit and sceNetInetTerm start and stop the library (wifi.c, netlink.c), the rest are
 * the sockets of the FTP server (netsock.c). Linking the SDK's libpspnet_inet for these makes psp-fixup-imports say "stubs out of order" (it does so in a three-line program
 * too), so they are imported here exactly as that library would: the same module, the same attribute (0x0009) and the same function numbers (read from the SDK's
 * libpspnet_inet.a). The number after the attribute is the number of functions below (0x000D = 13). */
	.set noreorder

#include "pspstub.s"

	STUB_START "sceNetInet",0x00090000,0x000d0005
	STUB_FUNC  0x17943399,sceNetInetInit
	STUB_FUNC  0xA9ED66B9,sceNetInetTerm
	STUB_FUNC  0xDB094E1B,sceNetInetAccept
	STUB_FUNC  0x1A33F9AE,sceNetInetBind
	STUB_FUNC  0x8D7284EA,sceNetInetClose
	STUB_FUNC  0x162E6FD5,sceNetInetGetsockname
	STUB_FUNC  0xD10A1A7A,sceNetInetListen
	STUB_FUNC  0xCDA85C99,sceNetInetRecv
	STUB_FUNC  0x5BE8D595,sceNetInetSelect
	STUB_FUNC  0x7AA671BC,sceNetInetSend
	STUB_FUNC  0x2FE71FE7,sceNetInetSetsockopt
	STUB_FUNC  0x8B7B220F,sceNetInetSocket
	STUB_FUNC  0xFBABE411,sceNetInetGetErrno
	STUB_END
