# Full-system smoke

Maps to **(not an official test)** in `../final-tests-guide.pdf`.

## What it exercises

A broad end-to-end run touching most instruction types at once. Useful as a quick health check.

## Workload

`initial_process.asm` spawns several copies of `child_process.asm` and exercises SET, SUM/SUB, JNZ, MEM_ALLOC/FREE, MOV_IN/OUT, MUTEX_*, SLEEP and STDIN/STDOUT.

## Key configuration

SCHEDULING_ALGORITHM=FIFO, QUEUE_ALGORITHMS=[FIFO,RR,RR,FIFO,RR,FIFO], QUEUE_PREEMPTION=FALSE; SEGMENT_MAX_SIZE=256; 6 memory sticks (1000..6000 B); 3 CPUs.

## Expected result

All modules connect and every instruction path runs without errors.

## Run

```sh
make full                 # kernel_memory :27182, kernel_scheduler :37031
make full MODE=memcheck
make kill
```
