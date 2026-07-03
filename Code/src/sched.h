#ifndef SCHED_H
#define SCHED_H

#include <stdbool.h>

#define MAX_PROC 1000
#define MAX_PID 32
#define MAX_GANTT 5000

// Process structure definition
typedef struct {
    char pid[MAX_PID];
    int arrival;
    int burst;
    int priority;
    
    int remaining;
    int start;
    int finish;
    int waiting;
    int turnaround;
    int response;
    bool is_started;
    bool is_finished;
} Process;

// Gantt segment structure
typedef struct {
    int start_time;
    char name[MAX_PID];
} GanttSegment;

// Function declarations
int cmp_arrival_pid(const void* a, const void* b);
void simulate(Process* procs, int n, const char* alg, int q, bool trace);
void run_demo(void);

#endif // SCHED_H
