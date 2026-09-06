# Short-term scheduling test

Covers the **Short-Term Scheduling** case from the course test guide (not committed to the repo).

## What it exercises

Multilevel-queue preemption and round-robin quantum handling.

## Workload

`SHORT_TERM.prc` spawns batches of `SHORT_TERM_1.prc` (prio 3), `SHORT_TERM_2.prc` (prio 2) and `SHORT_TERM_3.prc` (prio 1) interleaved with `NOOP`s, then exits.

## Key configuration

SCHEDULING_ALGORITHM=CMN, QUEUE_ALGORITHMS=[FIFO,RR,RR,RR], RR_QUANTUM=1500, SUSPENSION_TIMEOUT=35000; SEGMENT_MAX_SIZE=256; 1 memory stick (256 B); 1 CPU.

## Expected result

Processes are preempted according to the multilevel priorities. Processes under RR alternate execution respecting the quantum.

## Run

```sh
make short-term                 # kernel_memory :27169, kernel_scheduler :37018
make short-term MODE=memcheck
make kill
```
