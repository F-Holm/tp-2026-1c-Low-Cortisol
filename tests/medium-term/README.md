# Medium-term scheduling test

Covers the **Medium-Term Scheduling** case from the course test guide (not committed to the repo).

## What it exercises

Block/unblock transitions and suspension to swap while blocked.

## Workload

`MEDIUM_TERM.prc` spawns `MEDIUM_TERM_1..4.prc` spaced by `SLEEP 5000`, then exits. Enter a line of text at each STDIN prompt (the Makefile feeds `pseudocode/stdin_input.txt`).

## Key configuration

SCHEDULING_ALGORITHM=CMN, QUEUE_ALGORITHMS=[FIFO,FIFO,FIFO,FIFO], SUSPENSION_TIMEOUT=10000; SEGMENT_MAX_SIZE=128; 4 memory sticks (16, 16, 32, 64 B); 1 CPU.

## Expected result

Processes unblock as described in the assignment; blocked processes past the timeout are suspended to swap and resumed.

## Run

```sh
make medium-term                 # kernel_memory :27172, kernel_scheduler :37021
make medium-term MODE=memcheck
make kill
```
