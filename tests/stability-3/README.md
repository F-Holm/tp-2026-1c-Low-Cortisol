# General stability test 3

Covers the **General Stability** case from the course test guide (not committed to the repo).

## What it exercises

Same as `stability-1` with many short-lived copies of each workload.

## Workload

`STABILITY_3.prc` launches six copies each of `SHORT_TERM.prc`, `MEDIUM_TERM.prc`, `PRIORITY_INHERITANCE.prc` and `SCHED_MEM.prc`, then exits.

## Key configuration

Same as `stability-1`.

## Expected result

Same as `stability-1`.

## Run

```sh
make stability-3                 # kernel_memory :27180, kernel_scheduler :37029
make stability-3 MODE=memcheck
make kill
```
