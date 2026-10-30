# Лаб. 4. Вариант 10 – Приоритет писателей

Механизмы: Mutex, Semaphore (+ Event для одновременного старта потоков).

## Описание
Модель «Писатели–читатели». Если есть ожидающий писатель, новые читатели не допускаются;
уже читающие дорабатывают. Измеряется время ожидания читателей и писателей.
Конфигурации: 3 читателя/2 писателя и 3 читателя/5 писателей.

## Среда
Windows 10/11, MinGW (g++) или MSVC (cl).

## Сборка
MinGW: g++ -O2 -static -o ../build/lab04.exe src/main.cpp   (из папки lab04-sync)
MSVC:  cl /EHsc /O2 src\main.cpp /Fe:..\build\lab04.exe

## Параметры запуска
--readers N  --writers N  --ops N  --student-id NNN
--read-ms N  --write-ms N  --think-ms N  --timeout-s N
--verbose 0|1  --csv файл  --label имя
Если задан --student-id: SEED = 1000 + ID*97, OPERATIONS = 20 + ID*5.
Код возврата: 0 – нарушений инвариантов нет, 1 – есть, 2 – ошибка WinAPI, 3 – таймаут.

## Воспроизведение экспериментов
cd scripts
run_experiments.bat
python plot.py

## Тесты
cd tests && run_tests.bat

## Архитектура
- Общие ресурсы: dataA, dataB (должны быть равны), счётчики activeR/activeW/waitR/waitW.
- Потоки: N читателей, M писателей, главный поток.
- Mutex защищает счётчики. Semaphore readSem/writeSem блокируют ждущих читателей/писателей.
- Event hStart (manual-reset) даёт общий старт.
- Допуск передаётся «эстафетой»: счётчик active увеличивает тот, кто будит.