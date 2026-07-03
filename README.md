# CPT104 Courswork2: CPU Scheduling Simulator

A C99 CPU scheduling simulator for CPT104 Coursework 2. It implements the four required algorithms and one bonus algorithm:

- FCFS - First Come First Served (non-preemptive)
- SJF - Shortest Job First (non-preemptive)
- SRTF - Shortest Remaining Time First (preemptive)
- RR - Round Robin (preemptive)
- PRIO-A - priority scheduling with aging (bonus)

The program reads a workload file, prints a Gantt chart, optionally prints per-process metrics, and finishes with the required machine-readable summary block.

## Project structure

```text
.
|-- Makefile
|-- README.md
|-- src/
|   |-- sched.c
|   `-- sched.h
`-- tests/
    `-- workload.txt
```

Run all commands below from this directory. The compiled `sched` executable is generated in this directory and should not be included in the submission ZIP.

## Build

The project uses only standard C/POSIX libraries and is intended to compile on WebLinux.

```bash
make clean && make
```

The Makefile compiles with:

```text
gcc -Wall -Wextra -O2 -std=c99
```

To remove the generated executable:

```bash
make clean
```

## Workload format

Each non-comment line describes one process:

```text
PID ARRIVAL BURST [PRIORITY]
```

Example:

```text
# PID ARRIVAL BURST PRIORITY
P1 0 7 3
P2 2 4 1
P3 4 1 4
P4 6 4 2
```

- `ARRIVAL` must be a non-negative integer.
- `BURST` must be a positive integer.
- `PRIORITY` is optional for FCFS, SJF, SRTF, and RR, but required for PRIO-A.
- Blank lines and lines beginning with `#` are ignored.
- Invalid input produces an error message and a non-zero exit status.

Smaller numeric values represent higher priorities in PRIO-A.

## Usage

```bash
./sched <workload_file> --alg <algorithm> [--q <quantum>] [--trace]
```

Algorithm names are uppercase: `FCFS`, `SJF`, `SRTF`, `RR`, and `PRIO-A`.

Examples:

```bash
./sched tests/workload.txt --alg FCFS
./sched tests/workload.txt --alg SJF
./sched tests/workload.txt --alg SRTF --trace
./sched tests/workload.txt --alg RR --q 2 --trace
./sched tests/workload.txt --alg PRIO-A --trace
```

For RR, `--q` is mandatory and must be a positive integer. The optional `--trace` flag prints the per-process arrival, burst, priority, start, finish, waiting, turnaround, and response values.

## Demo mode

Demo mode needs no input file. It runs the built-in four-process workload with SRTF, RR (`q=2`), and PRIO-A, with per-process metrics enabled.

```bash
./sched --demo
```

## Scheduling rules

### Tie-breaking

Scheduling decisions are deterministic. When the algorithm's main comparison is tied, processes are selected by:

1. smaller arrival time;
2. smaller PID using lexicographic string comparison.

SJF first compares burst time, SRTF first compares remaining time, and RR uses a FIFO ready queue. Processes arriving simultaneously are placed into the RR queue in PID order.

### Priority aging

PRIO-A uses the following dynamic priority:

```text
dynamic priority = initial priority - (waiting time * 0.1)
```

This increases a waiting process's effective priority by 1 for every 10 time units it waits, helping reduce starvation.

### Context switches

This implementation counts every change after the initial dispatch:

| Transition | Counted? |
|---|:---:|
| Process -> Process | Yes |
| Process -> IDLE | Yes |
| IDLE -> Process | Yes |
| Initial state -> Process or IDLE | No |

Consecutive execution segments belonging to the same process do not add a context switch.

## Metrics and output

For each process:

```text
Turnaround = Finish - Arrival
Response   = Start - Arrival
Waiting    = Turnaround - Burst
```

CPU utilisation is calculated as:

```text
CPU_UTIL = total CPU busy time / makespan * 100%
```

Each run ends with the required summary format:

```text
RESULT: OK
ALG=SRTF
AVG_WAIT=...
AVG_TAT=...
AVG_RESP=...
CONTEXT_SWITCHES=...
CPU_UTIL=...%
```
