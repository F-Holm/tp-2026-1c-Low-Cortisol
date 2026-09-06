# Memory test (Worst Fit)

Covers the **Memory** case from the course test guide (not committed to the repo).

## What it exercises

Same as `mem-best`, with `ALLOCATION_STRATEGY=WORST`.

## Workload

Same script as `mem-best` (`SCHED_MEM.prc`).

## Key configuration

Same as `mem-best` except ALLOCATION_STRATEGY=WORST.

## Expected result

Memory is compacted under one of the two strategies. Values read back after compaction match what was written.

## Run

```sh
make mem-worst                 # kernel_memory :27171, kernel_scheduler :37020
make mem-worst MODE=memcheck
make kill
```
