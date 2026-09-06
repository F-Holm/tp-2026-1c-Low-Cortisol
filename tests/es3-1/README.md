# General stability test 1

Maps to **Prueba Estabilidad General** in `../final-tests-guide.pdf`.

## What it exercises

The whole system under sustained load: no busy-waiting, no memory leaks.

## Workload

`ES3_1.prc` launches one copy each of `PCP.prc`, `PHP.prc`, `PMP.prc`, `SCHED_MEM.prc`, `MEM_PRE_0.prc` and `SCHED_PRE_0.prc`, then exits.

## Key configuration

SCHEDULING_ALGORITHM=RR, QUEUE_ALGORITHMS=[RR x4], RR_QUANTUM=1500, SUSPENSION_TIMEOUT=35000; SEGMENT_MAX_SIZE=128; 4 memory sticks (2048 B each); 4 CPUs (the 3rd and 4th join after a delay).

## Expected result

No active waits (spinlocks) and no memory leaks. Run under `MODE=memcheck` / `MODE=helgrind` to check.

## Run

```sh
make es3-1                 # kernel_memory :27178, kernel_scheduler :37027
make es3-1 MODE=memcheck
make kill
```
