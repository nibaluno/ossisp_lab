#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdarg>

struct Config {
    int readers = 3, writers = 2;
    int ops = 20;
    int readMs = 20, writeMs = 40, thinkMs = 10;
    bool verbose = true;
};
static Config cfg;

static void ParseArgs(int argc, char** argv) {
    for (int i = 1; i + 1 < argc; i += 2) {
        const char* k = argv[i]; int v = atoi(argv[i + 1]);
        if (!strcmp(k, "--readers"))  cfg.readers = v;
        else if (!strcmp(k, "--writers"))  cfg.writers = v;
        else if (!strcmp(k, "--ops"))      cfg.ops = v;
        else if (!strcmp(k, "--read-ms"))  cfg.readMs = v;
        else if (!strcmp(k, "--write-ms")) cfg.writeMs = v;
        else if (!strcmp(k, "--think-ms")) cfg.thinkMs = v;
    }
}

static LARGE_INTEGER qpcFreq, qpcStart;
static double NowMs() {
    LARGE_INTEGER t; QueryPerformanceCounter(&t);
    return (double)(t.QuadPart - qpcStart.QuadPart) * 1000.0 / (double)qpcFreq.QuadPart;
}

enum Role { READER, WRITER };

struct Participant {
    int id; Role role; char name[16]; DWORD tid; unsigned rng;
};

static unsigned NextRand(unsigned& s) { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }

static CRITICAL_SECTION logCs;
static void Log(Participant* p, const char* fmt, ...) {
    if (!cfg.verbose) return;
    char buf[200]; va_list ap; va_start(ap, fmt); vsnprintf(buf, sizeof buf, fmt, ap); va_end(ap);
    EnterCriticalSection(&logCs);
    printf("[%06ld ms] %s | TID=%lu | %s\n", (long)NowMs(), p->name, (unsigned long)p->tid, buf);
    LeaveCriticalSection(&logCs);
}

static HANDLE hStart;

static DWORD WINAPI Worker(LPVOID arg) {
    Participant* p = (Participant*)arg;
    p->tid = GetCurrentThreadId();

    WaitForSingleObject(hStart, INFINITE);

    for (int i = 0; i < cfg.ops; i++) {
        Log(p, "STATE=THINKING");
        if (cfg.thinkMs > 0) Sleep(NextRand(p->rng) % (cfg.thinkMs + 1));

        Log(p, "STATE=WAITING op=%d", i + 1);

 

        if (p->role == READER) {
            Log(p, "STATE=READING");
            Sleep(cfg.readMs);
        }
        else {
            Log(p, "STATE=WRITING");
            Sleep(cfg.writeMs);
        }


    }
    Log(p, "STATE=FINISHED");
    return 0;
}

int main(int argc, char** argv) {
    ParseArgs(argc, argv);
    QueryPerformanceFrequency(&qpcFreq);
    QueryPerformanceCounter(&qpcStart);
    InitializeCriticalSection(&logCs);

    hStart = CreateEvent(NULL, TRUE, FALSE, NULL);

    int n = cfg.readers + cfg.writers;
    static Participant parts[64];
    HANDLE th[64];

    for (int i = 0; i < n; i++) {
        Participant* p = &parts[i];
        p->id = i; p->role = (i < cfg.readers) ? READER : WRITER;
        snprintf(p->name, sizeof p->name, "%c%d", p->role == READER ? 'R' : 'W',
            p->role == READER ? i + 1 : i - cfg.readers + 1);
        p->rng = 1000 * 2654435761u + (unsigned)(i + 1) * 7919u;

        th[i] = CreateThread(NULL, 0, Worker, p, 0, NULL);
    }

    QueryPerformanceCounter(&qpcStart);
    SetEvent(hStart);

    WaitForMultipleObjects(n, th, TRUE, INFINITE);

    for (int i = 0; i < n; i++) CloseHandle(th[i]);
    CloseHandle(hStart);
    DeleteCriticalSection(&logCs);

    return 0;
}