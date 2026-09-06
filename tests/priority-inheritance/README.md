# Priority inheritance test

Covers the **Priority Inheritance** case from the course test guide (not committed to the repo).

## What it exercises

Temporary priority changes when a low-priority process holds a mutex a higher-priority process needs.

## Workload

`PRIORITY_INHERITANCE.prc` creates `MUTEX_1/2/3`, then spawns `PRIORITY_INHERITANCE_1.prc` (prio 5) down to `PRIORITY_INHERITANCE_5.prc` (prio 1), with five copies of `PRIORITY_INHERITANCE_3.prc`. Wait until only the `PRIORITY_INHERITANCE_3.prc` processes remain.

## Key configuration

SCHEDULING_ALGORITHM=CMN, QUEUE_ALGORITHMS=[FIFO x6], SUSPENSION_TIMEOUT=1000000; SEGMENT_MAX_SIZE=128; 2 memory sticks (16, 16 B); 1 CPU.

## Expected result

Temporary priority changes (inheritance and its release) are visible in the kernel_scheduler log.

## Run

```sh
make priority-inheritance                 # kernel_memory :27176, kernel_scheduler :37025
make priority-inheritance MODE=memcheck
make kill
```
