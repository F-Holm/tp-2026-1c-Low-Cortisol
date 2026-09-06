# General stability test 2

Maps to **Prueba Estabilidad General** in `../final-tests-guide.pdf`.

## What it exercises

Same as `es3-1` with double the workload.

## Workload

`ES3_2.prc` launches the `es3-1` set twice, then exits.

## Key configuration

Same as `es3-1`.

## Expected result

Same as `es3-1`.

## Run

```sh
make es3-2                 # kernel_memory :27179, kernel_scheduler :37028
make es3-2 MODE=memcheck
make kill
```
