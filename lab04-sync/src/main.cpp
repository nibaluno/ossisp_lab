#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdarg>


struct Config {
    int readers = 3, writers = 2;
    int ops = 0;                 
    int baseOps = 20;          
    int studentId = -1;
    unsigned seed = 1000;
    bool opsSet = false;
    int readMs = 20, writeMs = 40, thinkMs = 10;
    int timeoutSec = 120;
    bool verbose = true;
    const char* csv = nullptr;
    const char* label = "run";
};
static Config cfg;

static void ParseArgs(int argc, char** argv) {
    for (int i = 1; i + 1 < argc; i += 2) {
        const char* k = argv[i]; int v = atoi(argv[i + 1]);
        if (!strcmp(k, "--readers"))    cfg.readers = v;
        else if (!strcmp(k, "--writers"))    cfg.writers = v;
        else if (!strcmp(k, "--ops")) { cfg.ops = v; cfg.opsSet = true; }
        else if (!strcmp(k, "--student-id")) cfg.studentId = v;
        else if (!strcmp(k, "--read-ms"))    cfg.readMs = v;
        else if (!strcmp(k, "--write-ms"))   cfg.writeMs = v;
        else if (!strcmp(k, "--think-ms"))   cfg.thinkMs = v;
        else if (!strcmp(k, "--timeout-s"))  cfg.timeoutSec = v;
        else if (!strcmp(k, "--verbose"))    cfg.verbose = v != 0;
        else if (!strcmp(k, "--csv"))        cfg.csv = argv[i + 1];
        else if (!strcmp(k, "--label"))      cfg.label = argv[i + 1];
        else { printf("Unknown option %s\n", k); exit(1); }
    }
    if (cfg.studentId >= 0) cfg.seed = 1000 + cfg.studentId * 97;        
    if (!cfg.opsSet) cfg.ops = cfg.baseOps + (cfg.studentId >= 0 ? cfg.studentId * 5 : 0);
    if (cfg.readers + cfg.writers > 60 || cfg.readers < 0 || cfg.writers < 0) {
        printf("Bad thread count\n"); exit(1);
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
    double waitSum, waitMax; int done;  
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

static void Fatal(const char* msg) {
    printf("FATAL: %s (GetLastError=%lu)\n", msg, (unsigned long)GetLastError());
    ExitProcess(2);
}


static volatile LONG dataA = 0, dataB = 0;          
static volatile LONG inReaders = 0, inWriters = 0; 
static volatile LONG violations = 0;


static HANDLE hMutex, hReadSem, hWriteSem;
static int activeR = 0, activeW = 0, waitR = 0, waitW = 0; 

static void Lock() {
    DWORD r = WaitForSingleObject(hMutex, 10000);
    if (r != WAIT_OBJECT_0 && r != WAIT_ABANDONED) Fatal("Mutex wait failed/timeout");
}
static void Unlock() { if (!ReleaseMutex(hMutex)) Fatal("ReleaseMutex"); }
static void WaitSem(HANDLE h) {
    if (WaitForSingleObject(h, (DWORD)cfg.timeoutSec * 1000) != WAIT_OBJECT_0) Fatal("Semaphore wait failed/timeout");
}
static void SignalSem(HANDLE h, LONG n) {
    if (!ReleaseSemaphore(h, n, NULL)) Fatal("ReleaseSemaphore");
}

static void StartRead() {
    Lock();
    if (activeW > 0 || waitW > 0) {      
        waitR++; Unlock();
        WaitSem(hReadSem);               
    }
    else { activeR++; Unlock(); }
}
static void EndRead() {
    Lock();
    activeR--;
    if (activeR == 0 && waitW > 0) {    
        waitW--; activeW = 1; SignalSem(hWriteSem, 1);
    }
    Unlock();
}
static void StartWrite() {
    Lock();
    if (activeR > 0 || activeW > 0) { waitW++; Unlock(); WaitSem(hWriteSem); }
    else { activeW = 1; Unlock(); }
}
static void EndWrite() {
    Lock();
    activeW = 0;
    if (waitW > 0) {                   
        waitW--; activeW = 1; SignalSem(hWriteSem, 1);
    }
    else if (waitR > 0) {              
        int k = waitR; activeR += k; waitR = 0; SignalSem(hReadSem, k);
    }
    Unlock();
}


static void DoRead() {
    InterlockedIncrement(&inReaders);
    if (inWriters != 0) InterlockedIncrement(&violations);   
    LONG a = dataA;
    Sleep(cfg.readMs / 2);
    LONG b = dataB;
    if (a != b) InterlockedIncrement(&violations);          
    Sleep(cfg.readMs - cfg.readMs / 2);
    InterlockedDecrement(&inReaders);
}
static void DoWrite() {
    LONG w = InterlockedIncrement(&inWriters);
    if (w != 1 || inReaders != 0) InterlockedIncrement(&violations); 
    LONG v = dataA + 1;
    dataA = v;
    Sleep(cfg.writeMs / 2);
    dataB = v;
    Sleep(cfg.writeMs - cfg.writeMs / 2);
    InterlockedDecrement(&inWriters);
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
        double t0 = NowMs();
        if (p->role == READER) StartRead(); else StartWrite();
        double w = NowMs() - t0;
        p->waitSum += w; if (w > p->waitMax) p->waitMax = w;

        if (p->role == READER) { Log(p, "STATE=READING"); DoRead(); }
        else { Log(p, "STATE=WRITING"); DoWrite(); }

        if (p->role == READER) EndRead(); else EndWrite();
        p->done++;
    }
    Log(p, "STATE=FINISHED");
    return 0;
}


int main(int argc, char** argv) {
    ParseArgs(argc, argv);
    QueryPerformanceFrequency(&qpcFreq);
    QueryPerformanceCounter(&qpcStart);
    InitializeCriticalSection(&logCs);

    hMutex = CreateMutex(NULL, FALSE, NULL);
    hReadSem = CreateSemaphore(NULL, 0, 64, NULL);
    hWriteSem = CreateSemaphore(NULL, 0, 64, NULL);
    hStart = CreateEvent(NULL, TRUE, FALSE, NULL);      
    if (!hMutex || !hReadSem || !hWriteSem || !hStart) Fatal("Create sync objects");

    int n = cfg.readers + cfg.writers;
    static Participant parts[64];
    HANDLE th[64];
    for (int i = 0; i < n; i++) {
        Participant* p = &parts[i];
        p->id = i; p->role = (i < cfg.readers) ? READER : WRITER;
        snprintf(p->name, sizeof p->name, "%c%d", p->role == READER ? 'R' : 'W',
            p->role == READER ? i + 1 : i - cfg.readers + 1);
        p->rng = cfg.seed * 2654435761u + (unsigned)(i + 1) * 7919u; if (!p->rng) p->rng = 1;
        th[i] = CreateThread(NULL, 0, Worker, p, 0, NULL);
        if (!th[i]) Fatal("CreateThread");
    }

    printf("Config: R=%d W=%d ops/thread=%d seed=%u read=%dms write=%dms think<=%dms\n",
        cfg.readers, cfg.writers, cfg.ops, cfg.seed, cfg.readMs, cfg.writeMs, cfg.thinkMs);

    QueryPerformanceCounter(&qpcStart);
    SetEvent(hStart);
    DWORD r = n ? WaitForMultipleObjects(n, th, TRUE, (DWORD)cfg.timeoutSec * 1000) : WAIT_OBJECT_0;
    double total = NowMs();
    if (r == WAIT_TIMEOUT) { printf("TIMEOUT: experiment exceeded %d s (deadlock?)\n", cfg.timeoutSec); ExitProcess(3); }
    if (r == WAIT_FAILED) Fatal("WaitForMultipleObjects");

    if (activeR || activeW || waitR || waitW || inReaders || inWriters) violations++; 
    if (dataA != dataB) violations++;
    if (dataA != (LONG)cfg.writers * cfg.ops) violations++;  

  
    double sumR = 0, sumW = 0, maxR = 0, maxW = 0; int cntR = 0, cntW = 0;
    for (int i = 0; i < n; i++) {
        Participant* p = &parts[i];
        if (p->role == READER) { sumR += p->waitSum; cntR += p->done; if (p->waitMax > maxR) maxR = p->waitMax; }
        else { sumW += p->waitSum; cntW += p->done; if (p->waitMax > maxW) maxW = p->waitMax; }
        printf("%s | TID=%lu: ops=%d avg_wait=%.2f ms max_wait=%.2f ms\n", p->name, (unsigned long)p->tid,
            p->done, p->done ? p->waitSum / p->done : 0.0, p->waitMax);
    }
    double avgR = cntR ? sumR / cntR : 0, avgW = cntW ? sumW / cntW : 0;
    printf("---- REPORT ----\n");
    printf("Total time        : %.1f ms\n", total);
    printf("Readers: ops=%d avg_wait=%.2f ms max_wait=%.2f ms\n", cntR, avgR, maxR);
    printf("Writers: ops=%d avg_wait=%.2f ms max_wait=%.2f ms\n", cntW, avgW, maxW);
    printf("Throughput        : %.1f ops/s\n", (cntR + cntW) * 1000.0 / total);
    printf("Invariant violations: %ld\n", (long)violations);

    if (cfg.csv) {
        FILE* f = fopen(cfg.csv, "a");
        if (f) {
            fseek(f, 0, SEEK_END);
            if (ftell(f) == 0)
                fprintf(f, "label,readers,writers,ops,seed,total_ms,avg_wait_r,avg_wait_w,max_wait_r,max_wait_w,violations\n");
            fprintf(f, "%s,%d,%d,%d,%u,%.1f,%.2f,%.2f,%.2f,%.2f,%ld\n", cfg.label, cfg.readers,
                cfg.writers, cfg.ops, cfg.seed, total, avgR, avgW, maxR, maxW, (long)violations);
            fclose(f);
        }
        else printf("Cannot open %s\n", cfg.csv);
    }

    for (int i = 0; i < n; i++) CloseHandle(th[i]);
    CloseHandle(hMutex); CloseHandle(hReadSem); CloseHandle(hWriteSem); CloseHandle(hStart);
    DeleteCriticalSection(&logCs);
    return violations ? 1 : 0;
}
