# Mid-term scheduling test (deterministic)

Maps to **Prueba Planificacion Mediano Plazo** in `../final-tests-guide.pdf`.

## What it exercises

Same as `pmp`, with a lower `INSTRUCTION_DELAY` (250) for a more reproducible run.

## Workload

Same script as `pmp` (`PMP.prc`).

## Key configuration

Same as `pmp` except INSTRUCTION_DELAY=250 (kernel_memory).

## Expected result

Same as `pmp`.

## Run

```sh
make pmp-det                 # kernel_memory :27173, kernel_scheduler :37022
make pmp-det MODE=memcheck
make kill
```
