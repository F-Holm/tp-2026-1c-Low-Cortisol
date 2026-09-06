# Base test (run 2)

Maps to **Prueba Base** in `../final-tests-guide.pdf`.

## What it exercises

Same as `base`, but with a memory-oriented workload.

## Workload

`MEM_PRE_0.prc` spawns `MEM_PRE_3.prc` (ends in a segmentation fault), sleeps, then spawns `MEM_PRE_1/2.prc`, then exits.

## Key configuration

Same as `base`.

## Expected result

All modules connect. `MEM_PRE_3` ends quickly by segmentation fault; the rest finish normally. No errors.

## Run

```sh
make base2                 # kernel_memory :27168, kernel_scheduler :37017
make base2 MODE=memcheck
make kill
```
