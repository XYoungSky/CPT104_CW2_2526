#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "sched.h"

// Sort by arrival time, if the same then sort by PID lexicographically (Tie-breaking rule)
int cmp_arrival_pid(const void* a, const void* b) {
    Process* p1 = (Process*)a;
    Process* p2 = (Process*)b;
    if (p1->arrival != p2->arrival) return p1->arrival - p2->arrival;
    return strcmp(p1->pid, p2->pid);
}

// Simulation core function (Event-driven style)
void simulate(Process* procs, int n, const char* alg, int q, bool trace) {
    qsort(procs, n, sizeof(Process), cmp_arrival_pid);

    int t = 0;
    int finished = 0;
    char prev_pid[MAX_PID] = "NONE";
    int context_switches = 0;
    int total_busy = 0;

    GanttSegment gantt[MAX_GANTT];
    int gantt_count = 0;

    // RR queue simulation (large enough array to avoid out-of-bounds)
    int queue[MAX_GANTT];
    int front = 0, rear = 0;
    bool in_queue[MAX_PROC] = {false};

    // At t=0, add all arrived processes to the RR queue
    if (strcmp(alg, "RR") == 0) {
        for (int i = 0; i < n; i++) {
            if (procs[i].arrival == 0) {
                queue[rear++] = i;
                in_queue[i] = true;
            }
        }
    }

    while (finished < n) {
        int selected = -1;
        int run_time = 0;

        // 1. Select process based on algorithm
        if (strcmp(alg, "FCFS") == 0 || strcmp(alg, "SJF") == 0) {
            int best = -1;
            for (int i = 0; i < n; i++) {
                if (!procs[i].is_finished && procs[i].arrival <= t) {
                    if (best == -1) {
                        best = i;
                    } else if (strcmp(alg, "SJF") == 0) {
                        if (procs[i].burst < procs[best].burst) best = i;
                    }
                }
            }
            if (best != -1) {
                selected = best;
                run_time = procs[selected].remaining; // Run until completion
            }
        } else if (strcmp(alg, "SRTF") == 0) {
            int best = -1;
            for (int i = 0; i < n; i++) {
                if (!procs[i].is_finished && procs[i].arrival <= t) {
                    if (best == -1 || procs[i].remaining < procs[best].remaining) {
                        best = i;
                    }
                }
            }
            if (best != -1) {
                selected = best;
                run_time = procs[selected].remaining;
                // Find the arrival time of the next potentially preempting process
                int next_arrival = 2147483647;
                for (int i = 0; i < n; i++) {
                    if (!procs[i].is_finished && procs[i].arrival > t && procs[i].arrival < next_arrival) {
                        next_arrival = procs[i].arrival;
                    }
                }
                if (t + run_time > next_arrival) {
                    run_time = next_arrival - t; // Event jump: pause and re-evaluate upon next process arrival
                }
            }
        } else if (strcmp(alg, "RR") == 0) {
            if (front < rear) {
                selected = queue[front++];
                in_queue[selected] = false;
                run_time = procs[selected].remaining > q ? q : procs[selected].remaining;
            }
        } else if (strcmp(alg, "PRIO-A") == 0) {
            // Priority with Aging: Preemptive priority + Aging mechanism
            int best = -1;
            double best_prio = 1e9; // Smaller value means higher priority
            
            for (int i = 0; i < n; i++) {
                if (!procs[i].is_finished && procs[i].arrival <= t) {
                    int run_time_so_far = procs[i].burst - procs[i].remaining;
                    int wait_time = (t - procs[i].arrival) - run_time_so_far;
                    
                    // Dynamic priority = Initial priority - (Wait time * Aging coefficient)
                    // Every 10 units of wait time, priority value decreases by 1 (i.e. priority increases)
                    double dyn_prio = procs[i].priority - (wait_time * 0.1);
                    
                    if (best == -1 || dyn_prio < best_prio) {
                        best = i;
                        best_prio = dyn_prio;
                    } else if (dyn_prio == best_prio) { // Tie-breaker
                        if (procs[i].arrival < procs[best].arrival) {
                            best = i;
                        }
                    }
                }
            }
            
            if (best != -1) {
                selected = best;
                run_time = procs[selected].remaining;
                
                int next_arrival = 2147483647;
                bool others_waiting = false;
                for (int i = 0; i < n; i++) {
                    if (!procs[i].is_finished) {
                        if (procs[i].arrival > t && procs[i].arrival < next_arrival) {
                            next_arrival = procs[i].arrival;
                        }
                        if (procs[i].arrival <= t && i != selected) {
                            others_waiting = true;
                        }
                    }
                }
                
                // Due to the aging mechanism, the priority of other waiting processes might surpass the current process at any second.
                // If there are other processes waiting, step 1 unit of time for precise recalculation.
                // Otherwise, jump directly to the next arrival event.
                if (others_waiting) {
                    run_time = 1;
                } else if (t + run_time > next_arrival) {
                    run_time = next_arrival - t; 
                }
            }
        }

        // 2. If no process is ready, CPU is idle
        if (selected == -1) {
            int next_arrival = 2147483647;
            for (int i = 0; i < n; i++) {
                if (!procs[i].is_finished && procs[i].arrival > t && procs[i].arrival < next_arrival) {
                    next_arrival = procs[i].arrival;
                }
            }
            run_time = next_arrival - t;
            
            if (strcmp(prev_pid, "IDLE") != 0) {
                if (strcmp(prev_pid, "NONE") != 0) context_switches++;
                strcpy(prev_pid, "IDLE");
                gantt[gantt_count].start_time = t;
                strcpy(gantt[gantt_count].name, "IDLE");
                gantt_count++;
            }
            t += run_time;

            if (strcmp(alg, "RR") == 0) {
                for (int i = 0; i < n; i++) {
                    if (procs[i].arrival > t - run_time && procs[i].arrival <= t && !procs[i].is_finished && !in_queue[i]) {
                        queue[rear++] = i;
                        in_queue[i] = true;
                    }
                }
            }
            continue;
        }

        // 3. Execute process
        Process* p = &procs[selected];
        if (!p->is_started) {
            p->start = t;
            p->response = p->start - p->arrival;
            p->is_started = true;
        }

        if (strcmp(prev_pid, p->pid) != 0) {
            if (strcmp(prev_pid, "NONE") != 0) context_switches++;
            strcpy(prev_pid, p->pid);
            gantt[gantt_count].start_time = t;
            strcpy(gantt[gantt_count].name, p->pid);
            gantt_count++;
        }

        int old_t = t;
        t += run_time;
        total_busy += run_time;
        p->remaining -= run_time;

        // RR queue maintenance: ensure newly arrived processes are enqueued first, then the preempted current process
        if (strcmp(alg, "RR") == 0) {
            for (int check_t = old_t + 1; check_t <= t; check_t++) {
                for (int i = 0; i < n; i++) {
                    if (procs[i].arrival == check_t && !procs[i].is_finished && !in_queue[i]) {
                        queue[rear++] = i;
                        in_queue[i] = true;
                    }
                }
            }
            if (p->remaining > 0) {
                queue[rear++] = selected;
                in_queue[selected] = true;
            }
        }

        // 4. Check if finished
        if (p->remaining == 0) {
            p->finish = t;
            p->turnaround = p->finish - p->arrival;
            p->waiting = p->turnaround - p->burst;
            p->is_finished = true;
            finished++;
        }
    }

    // Print output
    printf("\nGANTT:\n");
    for (int i = 0; i < gantt_count; i++) {
        printf("%d | %s | ", gantt[i].start_time, gantt[i].name);
    }
    printf("%d\n\n", t);

    double total_wait = 0, total_tat = 0, total_resp = 0;
    
    // Print each process's metrics ordered by original PID lexicographically (for unified aesthetics)
    Process sorted_print[MAX_PROC];
    memcpy(sorted_print, procs, n * sizeof(Process));
    for(int i=0; i<n-1; i++) {
        for(int j=i+1; j<n; j++) {
            if(strcmp(sorted_print[i].pid, sorted_print[j].pid) > 0) {
                Process temp = sorted_print[i];
                sorted_print[i] = sorted_print[j];
                sorted_print[j] = temp;
            }
        }
    }

    for (int i = 0; i < n; i++) {
        Process p = sorted_print[i];
        if (trace) {
            printf("PID: %s | Arr: %d | Burst: %d | Prio: %d | Start: %d | Finish: %d | Wait: %d | TAT: %d | Resp: %d\n",
                   p.pid, p.arrival, p.burst, p.priority, p.start, p.finish, p.waiting, p.turnaround, p.response);
        }
        total_wait += p.waiting;
        total_tat += p.turnaround;
        total_resp += p.response;
    }

    double util = t == 0 ? 0.0 : ((double)total_busy / t) * 100.0;

    printf("\nRESULT: OK\n");
    printf("ALG=%s\n", alg);
    printf("AVG_WAIT=%.2f\n", total_wait / n);
    printf("AVG_TAT=%.2f\n", total_tat / n);
    printf("AVG_RESP=%.2f\n", total_resp / n);
    printf("CONTEXT_SWITCHES=%d\n", context_switches);
    printf("CPU_UTIL=%.2f%%\n", util);
}

// Demo mode data
void run_demo() {
    // Field order: pid, arrival, burst, priority, remaining, start, finish, turnaround, waiting, response, is_started, is_finished
    Process base_procs[4] = {
        {"P1", 0, 7, 3, 7, 0, 0, 0, 0, 0, false, false},
        {"P2", 2, 4, 1, 4, 0, 0, 0, 0, 0, false, false},
        {"P3", 4, 1, 4, 1, 0, 0, 0, 0, 0, false, false},
        {"P4", 5, 4, 2, 4, 0, 0, 0, 0, 0, false, false}
    };
    Process procs[4];
    
    printf("=== DEMO MODE: SRTF ===\n");
    memcpy(procs, base_procs, sizeof(base_procs));
    simulate(procs, 4, "SRTF", 0, true);
    
    printf("\n=== DEMO MODE: RR (q=2) ===\n");
    memcpy(procs, base_procs, sizeof(base_procs));
    simulate(procs, 4, "RR", 2, true);

    printf("\n=== DEMO MODE: PRIO-A (Aging) ===\n");
    memcpy(procs, base_procs, sizeof(base_procs));
    simulate(procs, 4, "PRIO-A", 0, true);
}

int main(int argc, char* argv[]) {
    if (argc == 2 && strcmp(argv[1], "--demo") == 0) {
        run_demo();
        return 0;
    }

    char* filename = NULL;
    char* alg = NULL;
    int q = -1;
    bool trace = false;

    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--alg") == 0 && i + 1 < argc) {
            alg = argv[++i];
        } else if (strcmp(argv[i], "--q") == 0 && i + 1 < argc) {
            q = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--trace") == 0) {
            trace = true;
        } else if (argv[i][0] != '-') {
            filename = argv[i];
        }
    }

    if (!filename || !alg) {
        fprintf(stderr, "Error: Missing workload file or --alg argument.\n");
        fprintf(stderr, "Supported algs: FCFS, SJF, SRTF, RR, PRIO-A\n");
        return 1;
    }
    if (strcmp(alg, "RR") == 0 && q <= 0) {
        fprintf(stderr, "Error: Round Robin requires a positive time quantum --q.\n");
        return 1;
    }

    // Parse file
    FILE* fp = fopen(filename, "r");
    if (!fp) {
        perror("Error opening file");
        return 1;
    }

    Process procs[MAX_PROC];
    int n = 0;
    char line[256];

    while (fgets(line, sizeof(line), fp)) {
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;
        
        char pid[MAX_PID];
        int arr, brst, prio = 0; // Default priority is 0
        int parsed = sscanf(line, "%s %d %d %d", pid, &arr, &brst, &prio);
        
        if (parsed < 3) {
            fprintf(stderr, "Error: Invalid line format: %s\n", line);
            fclose(fp);
            return 1;
        }

        if (arr < 0) {
            fprintf(stderr, "Error: ARRIVAL must be a non-negative integer: %s\n", line);
            fclose(fp);
            return 1;
        }

        if (brst <= 0) {
            fprintf(stderr, "Error: BURST must be a positive integer: %s\n", line);
            fclose(fp);
            return 1;
        }
        
        if (strcmp(alg, "PRIO-A") == 0 && parsed < 4) {
            fprintf(stderr, "Error PRIO-A algorithm: Invalid line format: %s\n", line);
            fclose(fp);
            return 1;
        }
        
        strcpy(procs[n].pid, pid);
        procs[n].arrival = arr;
        procs[n].burst = brst;
        procs[n].priority = prio;
        procs[n].remaining = brst;
        procs[n].is_started = false;
        procs[n].is_finished = false;
        n++;
    }
    fclose(fp);

    if (n == 0) {
        fprintf(stderr, "Error: No valid processes found in file.\n");
        return 1;
    }

    simulate(procs, n, alg, q, trace);
    return 0;
}