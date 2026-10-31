# CSOPESY MO1 - Process Scheduler and CLI Emulator

* **Last Updated:** 10-31-2026
* **Group Members:**
  * Carlo Gabriel Barreo
  * Gaibril Kyle Gregorio
  * David Lorenz Javier
  * Tyrone Julius Lee

---

## 1. Overview

This program emulates a multi-core operating system process scheduler and command-line interpreter (CLI). It reads system parameters from a configuration file (`config.txt`), runs a real-time CPU scheduler on a dedicated background thread, creates processes on demand or in batches, and executes barebones instructions (`PRINT`, `DECLARE`, `ADD`, `SUBTRACT`, `SLEEP`, `FOR`) one CPU tick at a time.

* **Program Entry Point:** `main.cpp`

### Console Levels
The CLI operates across two levels:
* `root:\>` (or `root>`) — The main menu shell.
* Process `"screen"` (`root>process=smi `) — An attached view for a specific process.

When attaching to a process screen via `screen -s` or `screen -r`, the console clears and displays the process details. Inside a process screen, only `process-smi` and `exit` are recognized. Typing `exit` detaches from the screen and returns to the main menu prompt without terminating the emulator.

---

## 2. Project File Structure

| File | Description |
| :--- | :--- |
| `main.cpp` | **Program entry point** (handles CLI argument and environment variable parsing). |
| `CommandInterpreter.*` | Command recognition, dispatch, and `screen` creation/reattachment logic. |
| `ConsoleUI.*` | Welcome banner, process screen display, and utilization report formatting. |
| `Scheduler.*` | Background scheduler thread and real-time CPU tick timer. |
| `SchedulerCore.*` | Ready queue, multi-core allocation, FCFS/RR scheduling, and batch process generation. |
| `Process.*` | Process control block (PCB), instruction execution, `uint16` memory/registers, logs, and state. |
| `Instruction.*` | Instruction model and randomized instruction sequence generator. |
| `Config.*` | `config.txt` parser and parameter range validation. |
| `Util.*` | Helper utilities (string trimming, token parsing, and `MM/DD/YYYY HH:MM:SSAM` timestamps). |
| `tests.cpp` | Deterministic unit and self-test suite. |
| `build.bat` | Automated build script that compiles `csopesy.exe`. |
| `test.bat` | Automated script that compiles and runs `tests.exe`. |
| `config.txt` | System and scheduler configuration file. |
| `README.txt` / `README.md` | Project documentation and execution guide. |

---

## 3. System Requirements

* **Compiler:** C++17 compatible compiler (built and tested with **MinGW-w64 `g++`**).
* **Operating System:** Windows (uses the Win32 Console API for console handling).

---

## 4. Building the Program

### Option A: Automated Build Script (Recommended)
Run the batch script from the project root directory:
```bat
build.bat
```
This compiles and produces `csopesy.exe` in the project folder. The script searches for `g++` on the system `PATH` (with a fallback check). The executable is compiled with static linking (`-static -static-libgcc -static-libstdc++`) so it runs standalone without requiring external MinGW runtime DLLs.

### Option B: Manual Compilation Command
Run the following command in Command Prompt or PowerShell from the project root folder:
```bat
g++ -std=c++17 -O2 -Wall -Wextra -static -static-libgcc -static-libstdc++ -o csopesy.exe main.cpp Util.cpp Config.cpp Instruction.cpp Process.cpp SchedulerCore.cpp Scheduler.cpp ConsoleUI.cpp CommandInterpreter.cpp
```

---

## 5. Running the Program

Ensure `config.txt` is in the current working directory, then run:
```bat
csopesy.exe
```

At startup, the CSOPESY welcome banner is displayed and the main prompt appears.

> **Important:** The first command entered must be `initialize`. All other commands (except `exit`) are rejected until the system is initialized.

* **`initialize`**: Reads `config.txt`, validates and displays the loaded settings, and starts the CPU scheduler thread. Re-running `initialize` stops the active scheduler, reloads `config.txt`, and clears all previous running and finished processes. If `config.txt` is missing or contains invalid values, all errors are reported and the scheduler will not start.
* **`exit`**: Safely stops and joins the background scheduler thread before terminating the application.

---

## 6. CLI Commands Reference

### Main Menu Commands

* **`initialize`**
  Loads `config.txt` and starts the CPU scheduler. Must be called before any other command (aside from `exit`).

* **`screen -s <process_name>`**
  Creates a new process with a unique `<process_name>`, generates a random list of instructions between `min-ins` and `max-ins`, and attaches the console to the process's screen.

* **`screen -r <process_name>`**
  Reattaches the console to an existing active process (ready, running, or sleeping). If the process does not exist or has already finished execution, the console prints:
  `Process <process_name> not found.`

* **`screen -ls`**
  Displays current CPU utilization and a summary of all running and finished processes:
  ```text
  CPU utilization: <used>/<total> (%)
  Cores used: <n>
  Cores available: <n>
  --------------------------------------
  Running processes:
  <name> (<MM/DD/YYYY HH:MM:SSAM>)  Core: <n>  <current_line> / <total_lines>

  Finished processes:
  <name> (<MM/DD/YYYY HH:MM:SSAM>)  Finished   <total_lines> / <total_lines>
  --------------------------------------
  ```
  *(Note: Processes currently in the ready or sleeping state are listed with `Core: -`.)*

* **`scheduler-start`** *(Alias: `scheduler-test`)*
  Continuously generates new dummy processes (`p01`, `p02`, `p03`, ...) every `batch-process-freq` CPU cycles and places them into the ready queue.

* **`scheduler-stop`**
  Stops generating new dummy processes. Existing processes continue executing normally.

* **`report-util`**
  Generates the exact same utilization and process summary report as `screen -ls` and saves it to `csopesy-log.txt` in the current working directory, printing the output file path.

* **`help`**
  Displays the list of available console commands.

### Process Screen Commands (Inside `screen -s` or `screen -r`)

* **`process-smi`**
  Prints the current process state, ID, accumulated `PRINT` logs (with timestamps and core IDs), and instruction progress (`Current instruction line` / `Lines of code`). Does not clear the screen so previous updates remain visible. Once the process completes all instructions, it prints `Finished!` at the bottom.
* **`exit`**
  Closes the current process view and returns to the main menu console.

---

## 7. Configuration (`config.txt`)

`config.txt` uses a space-separated `<parameter> <value>` format (one pair per line). Blank lines and comments following `#` are ignored. Parameter names are case-insensitive, and the `scheduler` value may be enclosed in quotes.

| Parameter | Valid Range | Default | Description |
| :--- | :--- | :--- | :--- |
| `num-cpu` | $[1, 128]$ | `4` | Number of available CPU cores. |
| `scheduler` | `"fcfs"` \| `"rr"` | `"rr"` | CPU scheduling algorithm (First-Come, First-Served or Round-Robin). |
| `quantum-cycles` | $[1, 2^{32}]$ | `5` | Time slice (in CPU cycles) per core for the Round-Robin scheduler. |
| `batch-process-freq` | $[1, 2^{32}]$ | `1` | Frequency (in CPU cycles) at which `scheduler-start` creates new dummy processes. |
| `min-ins` | $[1, 2^{32}]$ | `1000` | Minimum number of instructions generated per process (`min-ins` $\le$ `max-ins`). |
| `max-ins` | $[1, 2^{32}]$ | `2000` | Maximum number of instructions generated per process. |
| `delay-per-exec` | $[0, 2^{32}]$ | `0` | Busy-wait delay (in CPU cycles) before executing the next instruction (`delays-per-exec` is also accepted). |

### Sample `config.txt`
```text
num-cpu 4
scheduler "rr"
quantum-cycles 5
batch-process-freq 1
min-ins 1000
max-ins 2000
delay-per-exec 0
```

---

## 8. Process Instructions & Scheduler Behavior

### Barebones Instructions
* **`PRINT(msg)`**: Logs a message (default: `"Hello world from <process_name>!"` or `"Value from: " + var`) with a timestamp and executing core ID.
* **`DECLARE(var, value)`**: Declares a `uint16` variable in the process memory table initialized to `value`.
* **`ADD(var1, var2/value, var3/value)`**: Computes $\text{var1} = \text{var2} + \text{var3}$. Undeclared variables are automatically declared and initialized to $0$.
* **`SUBTRACT(var1, var2/value, var3/value)`**: Computes $\text{var1} = \text{var2} - \text{var3}$. Undeclared variables are automatically declared and initialized to $0$.
* **`SLEEP(X)`**: Places the process into the `WAITING` state for $X$ (`uint8`: $0$ to $255$) CPU cycles and relinquishes its assigned CPU core.
* **`FOR([instructions], repeats)`**: Repeats a block of instructions for a given number of iterations. Supports nested loops up to **3 levels deep**.

### Execution & Memory Rules
* **Clamping (`uint16`)**: All process variables are unsigned 16-bit integers clamped to the range $[0, 65535]$. Additions saturate at $65535$ and subtractions clamp at $0$.
* **Execution Rate & Busy-Waiting**: When `delay-per-exec` is $0$, a running process executes one instruction per CPU tick. When `delay-per-exec` $> 0$, the process remains on its assigned core and busy-waits for that many CPU cycles before completing each instruction.
* **Memory Lifecycle**: Variables are stored in process memory during execution and released once the process finishes. Logs, timestamps, and line counts are retained so finished processes can be inspected in `screen -ls` and `report-util`.

### Scheduling Algorithms
* **First-Come, First-Served (`fcfs`)**: Non-preemptive; a process runs on its assigned core until it finishes all instructions or yields the CPU via `SLEEP`.
* **Round-Robin (`rr`)**: Preemptive; a process runs until it finishes, yields via `SLEEP`, or exhausts its time slice (`quantum-cycles`), at which point it is preempted and placed at the back of the ready queue.
* **Wake-up Handling**: Sleeping processes decrement their sleep counter each CPU tick and rejoin the back of the ready queue once their counter reaches $0$.

---

## 9. Timing Overrides & Reproducibility (Testing Utilities)

By default, the background scheduler advances 1 CPU tick every **50 ms**. For faster demonstrations or automated grading checks, you can override the tick duration via CLI flags or environment variables (valid range: `1` to `10000` ms):

```bat
csopesy.exe --tick-ms 5
```
Or via environment variables:
```bat
:: Command Prompt
set CSOPESY_TICK_MS=5

:: PowerShell
$env:CSOPESY_TICK_MS=5
```

To make random instruction generation deterministic across runs, set a custom RNG seed before launching:
```bat
set CSOPESY_SEED=123
```

---

## 10. Automated Unit Tests

To build and execute the self-test suite, run:
```bat
test.bat
```
This compiles `tests.exe` from `tests.cpp` and runs deterministic checks covering `config.txt` parsing, `uint16` arithmetic clamping, `PRINT` formatting, `SLEEP` state transitions, 3-level nested `FOR` loops, FCFS and RR quantum preemption, `scheduler-start`/`scheduler-stop` batch generation, `delay-per-exec` busy-waiting, and UI report formatting.
