# General stability test 1

Covers the **General Stability** case from the course test guide (not committed to the repo).

## What it exercises

The whole system under sustained load: no busy-waiting, no memory leaks.

## Workload

`STABILITY_1.prc` launches one copy each of `SHORT_TERM.prc`, `PRIORITY_INHERITANCE.prc`, `MEDIUM_TERM.prc`, `SCHED_MEM.prc`, `MEM_PRE_0.prc` and `SCHED_PRE_0.prc`, then exits.

## Key configuration

SCHEDULING_ALGORITHM=RR, QUEUE_ALGORITHMS=[RR x4], RR_QUANTUM=1500, SUSPENSION_TIMEOUT=35000; SEGMENT_MAX_SIZE=128; 4 memory sticks (2048 B each); 4 CPUs (the 3rd and 4th join after a delay).

## Expected result

No active waits (spinlocks) and no memory leaks. Run under `MODE=memcheck` / `MODE=helgrind` to check.

## Run

```sh
make stability-1                 # kernel_memory :27178, kernel_scheduler :37027
make stability-1 MODE=memcheck
make kill
```
