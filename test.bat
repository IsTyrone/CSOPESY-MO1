@echo off
REM Builds and runs the deterministic unit/self tests.

setlocal
cd /d "%~dp0"

set "GXX="
where g++ >nul 2>nul && set "GXX=g++"
if not defined GXX if exist "D:\School\C_CMD\bin\g++.exe" set "GXX=D:\School\C_CMD\bin\g++.exe"
if not defined GXX goto nocompiler

"%GXX%" -std=c++17 -O2 -Wall -Wextra -static -static-libgcc -static-libstdc++ -o tests.exe ^
    tests.cpp Util.cpp Config.cpp Instruction.cpp Process.cpp SchedulerCore.cpp ConsoleUI.cpp
if errorlevel 1 goto fail

echo.
tests.exe
if errorlevel 1 goto testfail

endlocal
exit /b 0

:nocompiler
echo g++ not found. Install MinGW or add g++ to PATH.
endlocal
exit /b 1

:fail
echo Test build failed.
endlocal
exit /b 1

:testfail
echo Tests failed.
endlocal
exit /b 1
