/* A DLL that only says hello and goodbye: proves an inject and an eject round trip.
 * Writes to %TEMP%\hotswap-probe.log. Build: gcc -shared -s -o probe.dll probe.c */
#include <windows.h>
#include <stdio.h>

static void say(const char *what)
{
    char path[MAX_PATH];
    GetTempPathA(sizeof path, path);
    strcat(path, "hotswap-probe.log");
    FILE *f = fopen(path, "a");
    if (!f) return;
    fprintf(f, "%s pid %lu\n", what, GetCurrentProcessId());
    fclose(f);
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID reserved)
{
    (void)h; (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) say("attach");
    if (reason == DLL_PROCESS_DETACH) say("detach");
    return TRUE;
}
