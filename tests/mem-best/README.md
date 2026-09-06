# Memory test (Best Fit)

Covers the **Memory** case from the course test guide (not committed to the repo).

## What it exercises

Segment allocation, read/write, deletion and compaction under the Best Fit hole-selection strategy.

## Workload

`SCHED_MEM.prc` allocates several segments, writes marker values, frees one, allocates more, then reads every segment back and exits.

## Key configuration

SCHEDULING_ALGORITHM=RR, RR_QUANTUM=1500; ALLOCATION_STRATEGY=BEST, SEGMENT_MAX_SIZE=128; 4 memory sticks (16, 32, 64, 128 B); 1 CPU.

## Expected result

Memory is compacted under one of the two strategies (compare with `mem-worst`). Values read back after compaction match what was written.

## Run

```sh
make mem-best                 # kernel_memory :27170, kernel_scheduler :37019
make mem-best MODE=memcheck
make kill
```
