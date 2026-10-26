/* psp5-authentic - see helper.h */
#include <pspkernel.h>
#include <stdio.h>
#include <string.h>
#include "helper.h"
#include "plugins.h"
#include "system.h"
#include "psp5time_prx.h" /* generated: psp5time_prx[] and size_psp5time_prx */

static void prx_path(char *out, int size)
{
    snprintf(out, size, "%s/" HELPER_NAME, PSP5_DATA_DIR);
}

/* 1 = the file has exactly the bytes that are inside psp5 */
static int same_file(const char *path)
{
    SceUID fd = sceIoOpen(path, PSP_O_RDONLY, 0);
    if (fd < 0)
        return 0;
    unsigned char buf[256];
    unsigned pos = 0;
    int same = 1;
    for (;;) {
        int n = sceIoRead(fd, buf, sizeof(buf));
        if (n <= 0)
            break;
        if (pos + (unsigned)n > size_psp5time_prx || memcmp(buf, psp5time_prx + pos, (size_t)n)) {
            same = 0;
            break;
        }
        pos += (unsigned)n;
    }
    sceIoClose(fd);
    return same && pos == size_psp5time_prx;
}

static int install(const char *path)
{
    if (same_file(path))
        return 1;
    sceIoMkdir(PSP5_DATA_DIR, 0777);
    SceUID fd = sceIoOpen(path, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
    if (fd < 0)
        return 0;
    int n = sceIoWrite(fd, psp5time_prx, size_psp5time_prx);
    sceIoClose(fd);
    return n == (int)size_psp5time_prx;
}

void helper_setup(void)
{
    if (!sys_has_ark())
        return;
    char path[64];
    prx_path(path, sizeof(path));
    if (!install(path))
        return; /* no lines for a file that is not there */
    plugins_load(sys_ark_dir());
    if (!plugins_helper_any(HELPER_NAME))
        plugins_helper_add(path);
}
