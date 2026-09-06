# General stability test 2

Covers the **General Stability** case from the course test guide (not committed to the repo).

## What it exercises

Same as `stability-1` with double the workload.

## Workload

`STABILITY_2.prc` launches the `stability-1` set twice, then exits.

## Key configuration

Same as `stability-1`.

## Expected result

Same as `stability-1`.

## Run

```sh
make stability-2                 # kernel_memory :27179, kernel_scheduler :37028
make stability-2 MODE=memcheck
make kill
```
