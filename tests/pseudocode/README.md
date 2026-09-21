# Pseudocode scripts

The `.prc` files in this directory are the workloads the end-to-end test
scenarios run. Each line is one instruction; `INIT_PROC <file> <priority>` spawns
another script as a child process.

`kernel_memory` resolves these names relative to its `SCRIPTS_BASEPATH`
(`./tests/pseudocode/`), and each `tests/<scenario>/test.mk` picks the entry
script through `INITIAL_PROCESS`.

`stdin_input.txt` is fed to the `STDIN` IO instance; every `STDIN` syscall reads
one line from it.

See `RESULTS.md` for the expected behaviour of each preliminary script, and
`tests/<scenario>/README.md` for what each scenario exercises.
