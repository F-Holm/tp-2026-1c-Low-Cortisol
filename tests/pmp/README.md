# Mid-term scheduling test

Maps to **Prueba Planificacion Mediano Plazo** in `../final-tests-guide.pdf`.

## What it exercises

Block/unblock transitions and suspension to swap while blocked.

## Workload

`PMP.prc` spawns `PMP_1..4.prc` spaced by `SLEEP 5000`, then exits. Enter a line of text at each STDIN prompt (the Makefile feeds `pseudocode/stdin_input.txt`).

## Key configuration

SCHEDULING_ALGORITHM=CMN, QUEUE_ALGORITHMS=[FIFO,FIFO,FIFO,FIFO], SUSPENSION_TIMEOUT=10000; SEGMENT_MAX_SIZE=128; 4 memory sticks (16, 16, 32, 64 B); 1 CPU.

## Expected result

Processes unblock as described in the assignment; blocked processes past the timeout are suspended to swap and resumed.

## Run

```sh
make pmp                 # kernel_memory :27172, kernel_scheduler :37021
make pmp MODE=memcheck
make kill
```
