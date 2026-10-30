@echo off
chcp 65001 >nul
setlocal
cd ..
if not exist data mkdir data
set CSV_FILE=data\results.csv
if exist %CSV_FILE% del %CSV_FILE%

echo Starting experiments... Data will be saved to %CSV_FILE%

:: Студак
set SID=36

:: Запускаем цикл от 1 до 5 писателей (при 3 читателях) для графика
for %%w in (1, 2, 3, 4, 5) do (
    echo --- 3 Readers, %%w Writers (3 runs) ---
    for /L %%i in (1,1,3) do (
        build\lab04.exe --readers 3 --writers %%w --student-id %SID% --csv %CSV_FILE% --label "sweep_W%%w" --verbose 0
    )
)

echo Experiments finished successfully!
pause