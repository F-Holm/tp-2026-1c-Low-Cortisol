# Medium-term scheduling test (v2)

Covers the **Medium-Term Scheduling** case from the course test guide (not committed to the repo).

## What it exercises

Same as `medium-term` with the alternative `MEDIUM_TERM_V2.prc` workload and 1000 B memory sticks.

## Workload

`MEDIUM_TERM_V2.prc` spawns `MEDIUM_TERM_1..4.prc` spaced by `SLEEP 8000`, then exits.

## Key configuration

Same scheduler/memory config as `medium-term`; 4 memory sticks (16, 16, 32, 64 B) with MEMORY_DELAY=1000.

## Expected result

Same as `medium-term`.

## Run

```sh
make medium-term-v2                 # kernel_memory :27174, kernel_scheduler :37023
make medium-term-v2 MODE=memcheck
make kill
```
