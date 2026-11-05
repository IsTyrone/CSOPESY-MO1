========================================================================
 CSOPESY MO1 - Process Scheduler and CLI Emulator
========================================================================

Group members:
  Carlo Barreo
  Gaibril Kyle
  David Javier
  Tyrone Lee

Last updated: 10-24-2026

------------------------------------------------------------------------
1. OVERVIEW
------------------------------------------------------------------------
This program emulates an operating-system process scheduler. It reads a
configuration file, keeps a scheduler running on its own thread, creates
processes on demand, and executes simple instructions (PRINT, DECLARE,
ADD, SUBTRACT, SLEEP, FOR) one tick at a time.

The entry point is main.cpp.

The console has two levels:

  root>                 the main shell
  root>process=smi      a process "screen"

After starting a process screen, the program prints the process details,
and the prompt changes to "root>process=smi ". Inside a screen only
"process-smi" and "exit" are accepted. "exit" closes the screen and
returns to the root prompt; it does not terminate the program.

------------------------------------------------------------------------
2. FILES
------------------------------------------------------------------------
  main.cpp               Program entry point (argument and env parsing)
  CommandInterpreter.*   Command dispatch and screen/attach logic
  ConsoleUI.*            Banner, screen and report formatting
  Scheduler.*            Background scheduler thread (tick timer)
  SchedulerCore.*        Ready queue, cores, FCFS/RR, batch generation
  Process.*              Instruction execution, registers, logs, state
  Instruction.*          Instruction model and random program generator
  Config.*               config.txt parser and validation
  Util.*                 Small helpers (trim, parsing, timestamps)
  tests.cpp              Deterministic unit/self tests
  build.bat              Builds csopesy.exe
  test.bat               Builds and runs tests.exe
  config.txt             Sample configuration file
  README.txt             This file

------------------------------------------------------------------------
3. REQUIREMENTS
------------------------------------------------------------------------
  C++17 compiler. This project was built and tested with MinGW-w64 g++.
  Windows (the console handling code uses the Win32 console API).

------------------------------------------------------------------------
4. BUILDING
------------------------------------------------------------------------
Option A - script (recommended):

  build.bat

This produces csopesy.exe in the project folder. The script looks for
g++ on PATH, then tries D:\School\C_CMD\bin\g++.exe. The executable is
built with static linking so it runs without MinGW runtime DLLs.

Option B - manual command:

  g++ -std=c++17 -O2 -Wall -Wextra -static -static-libgcc
      -static-libstdc++ -o csopesy.exe main.cpp Util.cpp Config.cpp
      Instruction.cpp Process.cpp SchedulerCore.cpp Scheduler.cpp
      ConsoleUI.cpp CommandInterpreter.cpp

------------------------------------------------------------------------
5. RUNNING
------------------------------------------------------------------------
  csopesy.exe

Run it from the folder that contains config.txt. At startup the banner is
printed and the prompt "root>" appears. The first command must be
"initialize"; every command except "exit" is rejected until then.

  initialize      Reads config.txt, prints the loaded settings, and starts
                  the scheduler. Re-running it stops the current
                  scheduler, reloads config.txt, and clears all previous
                  processes (running and finished).
  exit            Terminates the program. The scheduler thread is stopped
                  and joined before the process exits.

If config.txt is missing or invalid, initialize prints every problem it
found and does not start the scheduler. Numeric ranges follow the
specification; unknown parameters are ignored with a warning.

------------------------------------------------------------------------
6. COMMANDS
------------------------------------------------------------------------
  initialize
      Load config.txt and start the scheduler. Required before anything
      else. Prints the effective configuration.

  screen -ls
      Shows the system state:
        CPU utilization: <used>/<total> as a percentage
        Cores used: <n>
        Cores available: <n>
        Running processes:
            <name> (<start time>)  Core: <n>  <current line> / <lines>
        Finished processes:
            <name> (<start time>)  Finished  <lines> / <lines>
      Processes that are ready or sleeping are listed with "Core: -".
      Process start times use the MM/DD/YYYY HH:MM:SSAM format.

  screen -s <process name>
      Creates a process with the given name and attaches to it. The
      program is randomly generated with between min-ins and max-ins
      instructions. Names must be unique and are used exactly as typed.

  screen -r <process name>
      Attaches to a process that is still active (ready, running, or
      sleeping). If the process already finished, or never existed, the
      program prints:  Process <name> not found.

  scheduler-start
      Starts generating dummy processes, one every batch-process-freq
      arrival intervals (50 ms each), named p01, p02, p03, ... ("scheduler-test" is accepted
      as an alias).

  scheduler-stop
      Stops generating new dummy processes. Processes that already exist
      keep running.

  report-util
      Writes the same report shown by "screen -ls" to csopesy-log.txt in
      the current working directory and prints the absolute path.

  help
      Prints the list of commands.

Inside a process screen:

  process-smi     Reprints the process view with the latest logs and
                  progress. It does NOT clear the screen, so previous
                  output stays visible. When the process has finished it
                  shows "Finished!" instead of the instruction counters.
  exit            Closes the screen and returns to the root prompt.

------------------------------------------------------------------------
7. CONFIGURATION (config.txt)
------------------------------------------------------------------------
One "<parameter> <value>" pair per line. Blank lines and everything after
'#' are ignored. The scheduler value may be quoted. Parameter names are
case-insensitive.

  Parameter            Range          Default  Meaning
  -------------------  -------------  -------  ------------------------
  num-cpu              1 - 128        4        Number of CPU cores
  scheduler            fcfs | rr      rr       Scheduling algorithm
  quantum-cycles       1 - 2^32       5        RR time slice, in cycles
  batch-process-freq   1 - 2^32       1        50 ms intervals between dummy
                                                process creations
  min-ins              1 - 2^32       1000     Minimum instructions
  max-ins              1 - 2^32       2000     Maximum instructions
  delay-per-exec       0 - 2^32       0        Cycles a process busy-waits
                                                after each instruction

"delays-per-exec" is accepted as a spelling variant of "delay-per-exec".
min-ins must be less than or equal to max-ins.

Sample config.txt:

  num-cpu 4
  scheduler "rr"
  quantum-cycles 5
  batch-process-freq 1
  min-ins 1000
  max-ins 2000
  delay-per-exec 0

------------------------------------------------------------------------
8. PROCESS BEHAVIOR
------------------------------------------------------------------------
Valid instructions:

  PRINT <message>              Prints a literal message.
  PRINT "Value from: " <var>   Prints a literal prefix and a variable.
  DECLARE <var> <value>        Declares a variable and assigns a value.
  ADD <var> <var|number> <var|number>
                               Sums the operands into the first variable.
  SUBTRACT <var> <var|number> <var|number>
                               Subtracts, clamping at zero.
  SLEEP <cycles>               Moves the process to WAITING for N cycles
                               and frees its core.
  FOR <count> [ ... ]          Repeats the body. Nesting up to 3 levels is
                               supported by the interpreter and tested.

Details:

  - Variables are unsigned 16-bit values (0 - 65535). Results that
    exceed 65535 saturate at 65535; subtraction clamps at 0.
  - Using an undeclared variable reads 0.
  - Each process executes one instruction per CPU cycle (per tick).
  - A process SLEEPs for the given number of cycles. Sleep is tracked in
    cycles, not in wall-clock time.
  - When delay-per-exec is greater than 0, the process occupies its core
    and busy-waits for that many cycles after each completed instruction
    without advancing its instruction pointer.
  - Each process keeps up to the last 100 log lines so that long-running
    batch processes do not grow memory without bound.
  - When a process finishes, its variables and code are released (as the
    specification requires) and it is moved to the finished list. Its
    logs, start time, finish time, and line count are kept for reporting.
  - A finished process cannot be reattached; use "screen -ls" to see it.

Scheduling:

  - FCFS: a process runs until it finishes or sleeps. No preemption.
  - RR: a process is preempted after quantum-cycles cycles and placed at
    the back of the ready queue.
  - Sleeping processes are moved back to the ready queue when their
    sleep count reaches zero, so they rejoin at the back.
  - Dummy processes are generated at the end of every
    batch-process-freq-th arrival interval while the batch generator is running.
    Names that already exist are skipped (p01, p02, ... are incremented).

------------------------------------------------------------------------
9. TIMING OVERRIDES AND REPRODUCIBILITY
------------------------------------------------------------------------
The scheduler advances one CPU cycle every 1 ms by default. Arrivals use a separate 50 ms clock (batch-process-freq counts arrival intervals). Slow execution ticks allow at most one arrival per tick, without catch-up bursts. For other
demos or automated testing:

  csopesy.exe --tick-ms 5
  csopesy.exe --tick-ms=5

or set an environment variable before starting:

  set CSOPESY_TICK_MS=5      (cmd)
  $env:CSOPESY_TICK_MS=5     (PowerShell)

Accepted range is 1 - 10000 ms. The command line takes priority over the
environment variable.

Generated programs and dummy process sizes come from a random number
generator. Set CSOPESY_SEED to a non-zero value to make a run
reproducible:

  set CSOPESY_SEED=123

These options are conveniences for testing only; the program behaves
normally when neither is provided.

------------------------------------------------------------------------
10. TESTING
------------------------------------------------------------------------
  test.bat

Builds tests.exe from tests.cpp and the core modules (including the
console formatters, which are tested directly), then runs it.
The suite covers config parsing and validation, arithmetic and clamping,
PRINT variants, log capping, SLEEP, FOR nesting (1 to 3 levels),
instruction-template repetition, program generation bounds, FCFS and RR
scheduling, batch generation and naming, scheduler-stop, delay-per-exec,
snapshots, and the console formatting for process screens and the system
report.

Each test prints a single "Checks run: N, failures: 0" line and
"ALL TESTS PASSED" on success. The exit code is 0 on success and 1 on
failure.

------------------------------------------------------------------------
11. NOTES AND ASSUMPTIONS
------------------------------------------------------------------------
  - The banner, the "root>" and "root>process=smi " prompts, the process
    screen layout, and the report content follow the specification.
  - process-smi intentionally does not clear the screen, so logs stay
    visible between refreshes. Attaching to a new process does clear the
    screen, matching the specification's screen behavior.
  - The system report is always written to csopesy-log.txt in the
    working directory and is overwritten on each call.
  - Only "exit" terminates the program. "exit" inside a screen only
    closes that screen.
  - The scheduler runs on a background thread; the shell stays
    responsive while processes execute.
========================================================================
