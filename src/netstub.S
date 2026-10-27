/* psp5 – made by Mille Weber */
/* The two calls that start and stop the PSP's Inet library (sceNetInetInit / sceNetInetTerm), imported by hand. Linking the SDK's
 * libpspnet_inet for these two makes psp-fixup-imports say "stubs out of order" (it does so in a three-line program too), so they are
 * imported here exactly as that library would: the same module, the same attribute (0x0009) and the same function numbers (read from
 * the SDK's libpspnet_inet.a). Used by wifi.c, which starts the network libraries the way a game does before it shows the network dialog. */
	.set noreorder

#include "pspstub.s"

	STUB_START "sceNetInet",0x00090000,0x00020005
	STUB_FUNC  0x17943399,sceNetInetInit
	STUB_FUNC  0xA9ED66B9,sceNetInetTerm
	STUB_END
