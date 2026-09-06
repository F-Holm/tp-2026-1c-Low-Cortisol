# Medium-term scheduling test (deterministic)

Covers the **Medium-Term Scheduling** case from the course test guide (not committed to the repo).

## What it exercises

Same as `medium-term`, with a lower `INSTRUCTION_DELAY` (250) for a more reproducible run.

## Workload

Same script as `medium-term` (`MEDIUM_TERM.prc`).

## Key configuration

Same as `medium-term` except INSTRUCTION_DELAY=250 (kernel_memory).

## Expected result

Same as `medium-term`.

## Run

```sh
make medium-term-det                 # kernel_memory :27173, kernel_scheduler :37022
make medium-term-det MODE=memcheck
make kill
```
