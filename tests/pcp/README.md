# Short-term scheduling test

Maps to **Prueba Planificacion Corto Plazo** in `../final-tests-guide.pdf`.

## What it exercises

Multilevel-queue preemption and round-robin quantum handling.

## Workload

`PCP.prc` spawns batches of `PCP_1.prc` (prio 3), `PCP_2.prc` (prio 2) and `PCP_3.prc` (prio 1) interleaved with `NOOP`s, then exits.

## Key configuration

SCHEDULING_ALGORITHM=CMN, QUEUE_ALGORITHMS=[FIFO,RR,RR,RR], RR_QUANTUM=1500, SUSPENSION_TIMEOUT=35000; SEGMENT_MAX_SIZE=256; 1 memory stick (256 B); 1 CPU.

## Expected result

Processes are preempted according to the multilevel priorities. Processes under RR alternate execution respecting the quantum.

## Run

```sh
make pcp                 # kernel_memory :27169, kernel_scheduler :37018
make pcp MODE=memcheck
make kill
```
