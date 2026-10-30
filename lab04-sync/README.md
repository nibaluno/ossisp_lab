# Lab 4. Variant 10 - Writers Priority

Synchronization mechanisms: Mutex, Semaphore (+ Event for simultaneous thread start).

## Description
Readers-Writers model with writers priority. When a writer is waiting, new readers are blocked;
currently active readers finish their work. Measures waiting time for both readers and writers.
Investigated configurations: 3 readers / 2 writers and 3 readers / 5 writers.

## Environment
Windows 10/11, MinGW (g++) or MSVC (cl).

## Build Instructions
MinGW: g++ -O2 -static -o ../build/lab04.exe src/main.cpp   (run from lab04-sync folder)
MSVC:  cl /EHsc /O2 src\main.cpp /Fe:..\build\lab04.exe

## Command Line Options
--readers N  --writers N  --ops N  --student-id NNN
--read-ms N  --write-ms N  --think-ms N  --timeout-s N
--verbose 0|1  --csv FILE  --label NAME
If --student-id is set: SEED = 1000 + ID*97, OPERATIONS = 20 + ID*5.
Exit codes: 0 - no invariant violations, 1 - violation detected, 2 - WinAPI error, 3 - timeout.

## Reproducing Experiments
cd scripts
run_experiments.bat   (set your 3-digit STUDENT_ID inside the script)
python plot.py

## Running Tests
cd tests && run_tests.bat

## Architecture
- Shared resources: dataA, dataB (must remain equal), counters activeR/activeW/waitR/waitW.
- Threads: N readers, M writers, main thread.
- Mutex protects state counters. Semaphores readSem/writeSem block waiting readers/writers.
- Event hStart (manual-reset) triggers simultaneous start for all worker threads.
- Baton passing: the waking thread updates activeR/activeW before releasing the semaphore.
