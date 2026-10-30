@echo off
echo Running basic tests for Lab04...
..\build\lab04.exe --readers 3 --writers 2 --ops 10
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Invariant violations detected!
    exit /b %ERRORLEVEL%
)
echo [SUCCESS] Test passed, no violations!
pause
