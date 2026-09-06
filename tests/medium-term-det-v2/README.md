# Medium-term scheduling test (v2, deterministic)

Covers the **Medium-Term Scheduling** case from the course test guide (not committed to the repo).

## What it exercises

`medium-term-v2` with `INSTRUCTION_DELAY=250`.

## Workload

Same script as `medium-term-v2` (`MEDIUM_TERM_V2.prc`).

## Key configuration

Same as `medium-term-v2` except INSTRUCTION_DELAY=250.

## Expected result

Same as `medium-term`.

## Run

```sh
make medium-term-det-v2                 # kernel_memory :27175, kernel_scheduler :37024
make medium-term-det-v2 MODE=memcheck
make kill
```
