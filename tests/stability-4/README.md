# General stability test 4

Covers the **General Stability** case from the course test guide (not committed to the repo).

## What it exercises

Same as `stability-1`; the initial process loops on itself (`SET PC 0`) to keep the system busy.

## Workload

`STABILITY_4.prc` launches two copies each of `SHORT_TERM.prc`, `PRIORITY_INHERITANCE.prc` and `SCHED_MEM.prc`, sleeps, resets its program counter and exits.

## Key configuration

Same as `stability-1`.

## Expected result

Same as `stability-1`.

## Run

```sh
make stability-4                 # kernel_memory :27181, kernel_scheduler :37030
make stability-4 MODE=memcheck
make kill
```
