# Base test (run 1)

Maps to **Prueba Base** in `../final-tests-guide.pdf`.

## What it exercises

That every module starts, connects per the connectivity diagram, and that a mixed short-term workload runs to completion.

## Workload

`SCHED_PRE_0.prc` spawns children from `SCHED_PRE_1/2/3.prc` at priorities 3, 2 and 1, then exits.

## Key configuration

SCHEDULING_ALGORITHM=CMN, QUEUE_ALGORITHMS=[FIFO,RR,FIFO,RR], RR_QUANTUM=600, SUSPENSION_TIMEOUT=60000; ALLOCATION_STRATEGY=BEST, SEGMENT_MAX_SIZE=128; 1 memory stick (256 B); 1 CPU.

## Expected result

All modules connect successfully. Every process runs according to its instructions and finishes under the expected conditions. No errors.

## Run

```sh
make base                 # kernel_memory :27167, kernel_scheduler :37016
make base MODE=memcheck
make kill
```
