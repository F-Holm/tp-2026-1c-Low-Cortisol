# General stability test 4

Maps to **Prueba Estabilidad General** in `../final-tests-guide.pdf`.

## What it exercises

Same as `es3-1`; the initial process loops on itself (`SET PC 0`) to keep the system busy.

## Workload

`ES3_4.prc` launches two copies each of `PCP.prc`, `PHP.prc` and `SCHED_MEM.prc`, sleeps, resets its program counter and exits.

## Key configuration

Same as `es3-1`.

## Expected result

Same as `es3-1`.

## Run

```sh
make es3-4                 # kernel_memory :27181, kernel_scheduler :37030
make es3-4 MODE=memcheck
make kill
```
