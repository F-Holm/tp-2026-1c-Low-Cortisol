# Mid-term scheduling test (v2, deterministic)

Maps to **Prueba Planificacion Mediano Plazo** in `../final-tests-guide.pdf`.

## What it exercises

`pmp-v2` with `INSTRUCTION_DELAY=250`.

## Workload

Same script as `pmp-v2` (`PMP_v2.prc`).

## Key configuration

Same as `pmp-v2` except INSTRUCTION_DELAY=250.

## Expected result

Same as `pmp`.

## Run

```sh
make pmp-det-v2                 # kernel_memory :27175, kernel_scheduler :37024
make pmp-det-v2 MODE=memcheck
make kill
```
