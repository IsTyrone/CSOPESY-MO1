@echo off
REM Builds csopesy.exe with static linking so it can run on machines
REM without the MinGW runtime DLLs.

setlocal
cd /d "%~dp0"

set "GXX="
where g++ >nul 2>nul && set "GXX=g++"
if not defined GXX if exist "D:\School\C_CMD\bin\g++.exe" set "GXX=D:\School\C_CMD\bin\g++.exe"
if not defined GXX goto nocompiler

"%GXX%" -std=c++17 -O2 -Wall -Wextra -static -static-libgcc -static-libstdc++ -o csopesy.exe ^
    main.cpp Util.cpp Config.cpp Instruction.cpp Process.cpp SchedulerCore.cpp Scheduler.cpp ^
    ConsoleUI.cpp CommandInterpreter.cpp
if errorlevel 1 goto fail

echo.
echo Build OK: csopesy.exe
echo To run:   csopesy.exe
endlocal
exit /b 0

:nocompiler
echo g++ not found. Install MinGW or add g++ to PATH.
endlocal
exit /b 1

:fail
echo Build failed.
endlocal
exit /b 1
