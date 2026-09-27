/*
  ▄▄▄       ██ ▄█▀ ██▓▄▄▄█████▓ ▄▄▄             ██░ ██  ▒█████   ██▀███  ▄▄▄█████▓
 ▒████▄     ██▄█▒ ▓██▒▓  ██▒ ▓▒▒████▄          ▓██░ ██▒▒██▒  ██▒▓██ ▒ ██▒▓  ██▒ ▓▒
 ▒██  ▀█▄  ▓███▄░ ▒██▒▒ ▓██░ ▒░▒██  ▀█▄        ▒██▀▀██░▒██░  ██▒▓██ ░▄█ ▒▒ ▓██░ ▒░
 ░██▄▄▄▄██ ▓██ █▄ ░██░░ ▓██▓ ░ ░██▄▄▄▄██       ░▓█ ░██ ▒██   ██░▒██▀▀█▄  ░ ▓██▓ ░
  ▓█   ▓██▒▒██▒ █▄░██░  ▒██▒ ░  ▓█   ▓██▒      ░▓█▒░██▓░ ████▓▒░░██▓ ▒██▒  ▒██▒ ░
  ▒▒   ▓▒█░▒ ▒▒ ▓▒░▓    ▒ ░░    ▒▒   ▓▒█░       ▒ ░░▒░▒░ ▒░▒░▒░ ░ ▒▓ ░▒▓░  ▒ ░░
   ▒   ▒▒ ░░ ░▒ ▒░ ▒ ░    ░      ▒   ▒▒ ░       ▒ ░▒░ ░  ░ ▒ ▒░   ░▒ ░ ▒░    ░
   ░   ▒   ░ ░░ ░  ▒ ░  ░        ░   ▒          ░  ░░ ░░ ░ ░ ▒    ░░   ░   ░
       ░  ░░  ░    ░                 ░  ░       ░  ░  ░    ░ ░     ░
*/
/* hotswap: load a DLL into a running process, or unload one, by remote thread.
 *
 *   hotswap inject <process.exe> <path\to\mod.dll>
 *   hotswap eject  <process.exe> <mod.dll>
 *   hotswap list   <process.exe>
 *
 * inject: the DLL's path is written into the target and LoadLibraryA is run there
 * on a thread of ours. eject: FreeLibrary on the module's handle, the same way.
 * kernel32 sits at one address in every process, so our LoadLibraryA is theirs.
 * The DLL must have undone its hooks before an eject, or the next call into it
 * lands on unmapped memory; a DLL that unloads itself needs no eject at all.
 */
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <string.h>

static DWORD find_process(const char *exe)
{
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32 pe = { .dwSize = sizeof pe };
    DWORD pid = 0;
    for (BOOL ok = Process32First(snap, &pe); ok; ok = Process32Next(snap, &pe))
        if (_stricmp(pe.szExeFile, exe) == 0) { pid = pe.th32ProcessID; break; }
    CloseHandle(snap);
    return pid;
}

static HMODULE find_module(DWORD pid, const char *dll)
{
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snap == INVALID_HANDLE_VALUE) return NULL;
    MODULEENTRY32 me = { .dwSize = sizeof me };
    HMODULE h = NULL;
    for (BOOL ok = Module32First(snap, &me); ok; ok = Module32Next(snap, &me))
        if (_stricmp(me.szModule, dll) == 0) { h = me.hModule; break; }
    CloseHandle(snap);
    return h;
}

static int list_modules(DWORD pid)
{
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snap == INVALID_HANDLE_VALUE) return 1;
    MODULEENTRY32 me = { .dwSize = sizeof me };
    for (BOOL ok = Module32First(snap, &me); ok; ok = Module32Next(snap, &me))
        printf("%p  %s\n", (void *)me.hModule, me.szExePath);
    CloseHandle(snap);
    return 0;
}

/* Run kernel32!<fn>(arg) on a new thread inside the target and wait for it. */
static int run_remote(HANDLE proc, const char *fn, LPVOID arg, DWORD *ret)
{
    FARPROC f = GetProcAddress(GetModuleHandleA("kernel32.dll"), fn);
    if (!f) return 1;
    HANDLE t = CreateRemoteThread(proc, NULL, 0, (LPTHREAD_START_ROUTINE)(void *)f, arg, 0, NULL);
    if (!t) return 1;
    WaitForSingleObject(t, 10000);
    GetExitCodeThread(t, ret);
    CloseHandle(t);
    return 0;
}

static int inject(HANDLE proc, const char *dll)
{
    char full[MAX_PATH];
    if (!GetFullPathNameA(dll, sizeof full, full, NULL)) return 1;
    if (GetFileAttributesA(full) == INVALID_FILE_ATTRIBUTES) { fprintf(stderr, "no such file: %s\n", full); return 1; }
    SIZE_T n = strlen(full) + 1;
    LPVOID mem = VirtualAllocEx(proc, NULL, n, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!mem) return 1;
    int rc = 1;
    DWORD ret = 0;
    if (WriteProcessMemory(proc, mem, full, n, NULL) && run_remote(proc, "LoadLibraryA", mem, &ret) == 0)
        rc = ret ? 0 : 1;          /* LoadLibraryA returns the HMODULE, 0 on failure */
    VirtualFreeEx(proc, mem, 0, MEM_RELEASE);
    printf(rc ? "inject failed: %s\n" : "injected %s\n", full);
    return rc;
}

static int eject(HANDLE proc, DWORD pid, const char *dll)
{
    HMODULE h = find_module(pid, dll);
    if (!h) { fprintf(stderr, "not loaded: %s\n", dll); return 1; }
    DWORD ret = 0;
    if (run_remote(proc, "FreeLibrary", h, &ret) || !ret) { printf("eject failed: %s\n", dll); return 1; }
    printf("ejected %s\n", dll);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 3) { fprintf(stderr, "usage: hotswap inject|eject|list <process.exe> [dll]\n"); return 2; }
    const char *verb = argv[1], *exe = argv[2], *dll = argc > 3 ? argv[3] : NULL;
    DWORD pid = find_process(exe);
    if (!pid) { fprintf(stderr, "not running: %s\n", exe); return 1; }
    if (strcmp(verb, "list") == 0) return list_modules(pid);
    if (!dll) { fprintf(stderr, "usage: hotswap %s <process.exe> <dll>\n", verb); return 2; }
    HANDLE proc = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ, FALSE, pid);
    if (!proc) { fprintf(stderr, "OpenProcess failed (%lu); run as the same user, or elevated\n", GetLastError()); return 1; }
    int rc;
    if (strcmp(verb, "inject") == 0) rc = inject(proc, dll);
    else if (strcmp(verb, "eject") == 0) rc = eject(proc, pid, dll);
    else { fprintf(stderr, "unknown verb: %s\n", verb); rc = 2; }
    CloseHandle(proc);
    return rc;
}
