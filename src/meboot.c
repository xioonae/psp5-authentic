/* psp5 authentic – the Media Engine's firmware image for the hardware video decoder (see meboot.h) */
#include "meboot.h"

#include <pspkernel.h>
#include <kubridge.h>
#include <systemctrl.h>

#include "launch.h"

static u32 s_func;
static unsigned s_nid;
static int s_tried;
static int s_cur;

/* sceMeCore_driver's sceMeBootStart in the kernels' NID tables: 6.60 / 6.61 first (ARK runs on those), then the older ones, which ARK translates, when the first is not found */
static const unsigned NIDS[] = { 0x5DFF5C50, 0x99E4DBFA, 0x3A2E60BB, 0x47DB48C2 };
static const char *const MODS[] = { "sceMeCore_Driver", "sceMeCore_driver" };

static void resolve(void)
{
    if (s_tried)
        return;
    s_tried = 1;
    if (!launch_supported())
        return;
    for (unsigned n = 0; n < sizeof(NIDS) / sizeof(NIDS[0]) && !s_func; n++)
        for (unsigned m = 0; m < sizeof(MODS) / sizeof(MODS[0]) && !s_func; m++) {
            s_func = sctrlHENFindFunction(MODS[m], "sceMeCore_driver", NIDS[n]);
            if (s_func)
                s_nid = NIDS[n];
        }
}

int meboot_available(void)
{
    resolve();
    return s_func != 0;
}

unsigned meboot_nid(void) { return s_nid; }

int meboot_start(int type)
{
    resolve();
#ifdef PSP5_TEST /* (PPSSPP has no ARK and no Engine to start: the rest of the route is what its tests are for) */
    if (!s_func && type >= 0) {
        s_cur = type;
        return 0;
    }
#endif
    if (!s_func || type < 0)
        return -1;
    if (type == s_cur)
        return 0;
    KernelCallArg a = { 0 };
    a.arg1 = (u32)type;
    int r = kuKernelCall((void *)s_func, &a);
    if (r < 0)
        return r;
    if ((int)a.ret1 < 0)
        return (int)a.ret1;
    s_cur = type;
    return 0;
}

int meboot_current(void) { return s_cur; }
