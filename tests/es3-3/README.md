# General stability test 3

Maps to **Prueba Estabilidad General** in `../final-tests-guide.pdf`.

## What it exercises

Same as `es3-1` with many short-lived copies of each workload.

## Workload

`ES3_3.prc` launches six copies each of `PCP.prc`, `PMP.prc`, `PHP.prc` and `SCHED_MEM.prc`, then exits.

## Key configuration

Same as `es3-1`.

## Expected result

Same as `es3-1`.

## Run

```sh
make es3-3                 # kernel_memory :27180, kernel_scheduler :37029
make es3-3 MODE=memcheck
make kill
```
