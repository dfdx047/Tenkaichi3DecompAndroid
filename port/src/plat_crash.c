/*
 * Crash report for the PC build: on a fatal signal, writes the signal and a backtrace (function names; the build
 * is linked without PIE and with its symbols) to the terminal and to bt3_crash.txt in the current directory, then
 * lets the default action happen. So a crash someone else hits can be diagnosed from one file.
 */
#ifdef _WIN32
/* Windows: the exception code, the faulting address and the return addresses on the stack. The program is linked
   at a fixed address (0x20000000), so the numbers can be looked up in bt3.exe.map of the same build. */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>

extern const char *volatile gPortStage; /* plat_mem.c: how far the start got */

/* The report is written with the system's own file calls and no C library (the crash may be inside it), from a
   VECTORED handler: those are called first, before the system checks the stack, so a crash on a stack the system
   does not like (the game runs on its own stack below 4 GB) is still reported. The handler only reports; what
   happens to the exception afterwards is unchanged. */
static HANDLE sCrashOut[2];

static void crash_put(const char *s) {
    DWORD n = 0, done, k;
    while (s[n] != '\0') {
        n++;
    }
    for (k = 0; k < 2; k++) {
        if (sCrashOut[k] != NULL && sCrashOut[k] != INVALID_HANDLE_VALUE) {
            WriteFile(sCrashOut[k], s, n, &done, NULL);
        }
    }
}

static void crash_hex(unsigned long long v) {
    char b[20];
    int i;
    b[0] = '0'; b[1] = 'x';
    for (i = 0; i < 16; i++) {
        b[2 + i] = "0123456789ABCDEF"[(v >> (60 - i * 4)) & 15];
    }
    b[18] = '\0';
    crash_put(b);
}

/* " (in <module file> + offset)" for an address */
static void crash_where(void *addr) {
    HMODULE m = NULL;
    char name[MAX_PATH];
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)addr, &m) && m != NULL &&
        GetModuleFileNameA(m, name, sizeof(name)) != 0) {
        crash_put(" (");
        crash_put(name);
        crash_put(" + ");
        crash_hex((unsigned long long)((char *)addr - (char *)m));
        crash_put(")");
    }
}

static LONG WINAPI on_crash(EXCEPTION_POINTERS *e) {
    static LONG once;
    DWORD code = e->ExceptionRecord->ExceptionCode;
    NT_TIB *tib = (NT_TIB *)NtCurrentTeb();
    void *frames[32];
    USHORT n, i;

    if (code != EXCEPTION_ACCESS_VIOLATION && code != EXCEPTION_ILLEGAL_INSTRUCTION && code != EXCEPTION_STACK_OVERFLOW &&
        code != EXCEPTION_INT_DIVIDE_BY_ZERO && code != EXCEPTION_PRIV_INSTRUCTION && code != EXCEPTION_IN_PAGE_ERROR) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    if (InterlockedExchange(&once, 1) != 0) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    sCrashOut[0] = GetStdHandle(STD_ERROR_HANDLE);
    sCrashOut[1] = CreateFileA("bt3_crash.txt", GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    crash_put("bt3: crashed: exception ");
    crash_hex(code);
    crash_put(" at ");
    crash_hex((unsigned long long)(uintptr_t)e->ExceptionRecord->ExceptionAddress);
    crash_where(e->ExceptionRecord->ExceptionAddress);
    if (code == EXCEPTION_ACCESS_VIOLATION && e->ExceptionRecord->NumberParameters >= 2) {
        crash_put(e->ExceptionRecord->ExceptionInformation[0] == 1 ? "\r\nwriting address " : e->ExceptionRecord->ExceptionInformation[0] == 8 ? "\r\nexecuting address " : "\r\nreading address ");
        crash_hex(e->ExceptionRecord->ExceptionInformation[1]);
    }
    crash_put("\r\nlast step reached: ");
    crash_put(gPortStage);
#ifdef __x86_64__
    crash_put("\r\nrsp "); crash_hex(e->ContextRecord->Rsp);
    crash_put(" rbp "); crash_hex(e->ContextRecord->Rbp);
    crash_put("\r\nrax "); crash_hex(e->ContextRecord->Rax);
    crash_put(" rbx "); crash_hex(e->ContextRecord->Rbx);
    crash_put(" rcx "); crash_hex(e->ContextRecord->Rcx);
    crash_put(" rdx "); crash_hex(e->ContextRecord->Rdx);
    crash_put("\r\nrsi "); crash_hex(e->ContextRecord->Rsi);
    crash_put(" rdi "); crash_hex(e->ContextRecord->Rdi);
    crash_put(" r8 "); crash_hex(e->ContextRecord->R8);
    crash_put(" r9 "); crash_hex(e->ContextRecord->R9);
#endif
    crash_put("\r\nthread stack as the system knows it: ");
    crash_hex((unsigned long long)(uintptr_t)tib->StackLimit);
    crash_put(" to ");
    crash_hex((unsigned long long)(uintptr_t)tib->StackBase);
    crash_put("\r\nbacktrace (innermost first; addresses in Tenkaichi3Decomp.exe are looked up in the build's map file):\r\n");
    n = RtlCaptureStackBackTrace(0, 32, frames, NULL);
    for (i = 0; i < n; i++) {
        crash_hex((unsigned long long)(uintptr_t)frames[i]);
        crash_where(frames[i]);
        crash_put("\r\n");
    }
    crash_put("bt3: the same report is in bt3_crash.txt\r\n");
    if (sCrashOut[1] != INVALID_HANDLE_VALUE) {
        CloseHandle(sCrashOut[1]);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

/* (priority 101: before the other start-up code of the port, which reads the game data and reserves memory) */
__attribute__((constructor(101))) static void crash_init(void) {
    /* The program is a window program (no console window of its own). Started from a terminal, its messages go
       to that terminal; started by the setup with a pipe, the pipe is kept. */
    if (GetStdHandle(STD_OUTPUT_HANDLE) == NULL && AttachConsole(ATTACH_PARENT_PROCESS)) {
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
    }
    /* nothing held back in a buffer: what was printed before a crash is in the log */
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    AddVectoredExceptionHandler(1, on_crash);
}
#elif defined(__ANDROID__)
/* Android: no handlers of our own. The Java runtime of the app's process uses SIGSEGV itself (null checks, stack
   overflow), and the system's crash reporter (debuggerd) already writes the backtrace, with function names, to the
   log (adb logcat) and to a tombstone. */
#else
#include <execinfo.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

static void put(int fd, const char *s) {
    if (write(fd, s, strlen(s)) < 0) {
    }
}

static void on_crash(int sig) {
    void *frames[48];
    int n = backtrace(frames, 48), fd = open("bt3_crash.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644), k;
    const char *name = sig == SIGSEGV ? "SIGSEGV (bad memory access)" : sig == SIGFPE ? "SIGFPE (arithmetic)" :
                       sig == SIGILL ? "SIGILL (bad instruction)" : sig == SIGBUS ? "SIGBUS" : "SIGABRT";

    for (k = 0; k < 2; k++) {
        int out = k == 0 ? 2 : fd;
        if (out < 0) {
            continue;
        }
        put(out, "bt3: crashed: ");
        put(out, name);
        put(out, "\nbacktrace (innermost first):\n");
        backtrace_symbols_fd(frames, n, out);
    }
    if (fd >= 0) {
        close(fd);
        put(2, "bt3: the same report is in bt3_crash.txt\n");
    }
    signal(sig, SIG_DFL);
    raise(sig);
}

__attribute__((constructor)) static void crash_init(void) {
    static const int sigs[] = {SIGSEGV, SIGFPE, SIGILL, SIGBUS, SIGABRT};
    unsigned i;

    for (i = 0; i < sizeof(sigs) / sizeof(sigs[0]); i++) {
        signal(sigs[i], on_crash);
    }
}
#endif
