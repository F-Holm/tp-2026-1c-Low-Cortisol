# CPU

Simulates the instruction cycle of a simplified CPU: it interprets and executes
the pseudocode instructions of the Process the Kernel Scheduler assigns to it.

## Running

```sh
./bin/cpu [config file] [identifier]
```

The identifier distinguishes this CPU from the others and appears in its log
file. On startup the CPU connects to the Kernel Scheduler, to Kernel Memory, and
to every Memory Stick (more sticks may connect later, so Kernel Memory and the
CPUs coordinate to keep that list current). It then waits for a PID from the
Kernel Scheduler, requests the execution context from Kernel Memory, and starts
the first instruction cycle.

## Registers

| Register | Size | Type | Purpose |
|----------|------|------|---------|
| `PC` | 4 B | `uint32_t` | Program counter — next instruction to fetch. |
| `AX` `BX` `CX` `DX` | 1 B | `uint8_t` | General-purpose numeric registers. |
| `EAX` `EBX` `ECX` `EDX` | 4 B | `uint32_t` | General-purpose numeric registers. |
| `SI` | 4 B | `uint32_t` | Source logical address. |
| `DI` | 4 B | `uint32_t` | Destination logical address. |

## Instruction cycle

1. **Fetch** — request instruction number `PC` from Kernel Memory.
2. **Decode** — determine the instruction and whether it needs address
   translation.
3. **Execute** — see below.
4. **Check Interrupt** — if the Kernel Scheduler sent an interrupt for the
   running PID, update the context in Kernel Memory and return the PID with the
   interrupt reason; otherwise discard it.

`PC` is incremented by 1 at the end of the cycle unless the instruction changed
it.

### Executed by the CPU
`NOOP`, `SET`, `MOV_IN`, `MOV_OUT`, `SUM`, `SUB`, `JNZ`, `COPY_MEM`.

### Syscalls (delegated to the Kernel Scheduler)
`MUTEX_CREATE`, `MUTEX_LOCK`, `MUTEX_UNLOCK`, `MEM_ALLOC`, `MEM_FREE`, `SLEEP`,
`STDOUT`, `STDIN`, `INIT_PROC`, `EXIT`. The CPU increments `PC`, pushes the
updated context to Kernel Memory, and hands the PID back to the scheduler.

## MMU

Logical addresses are interpreted as `[segment number | offset]` in decimal:

```
segment_number = floor(logical_address / max_segment_size)
segment_offset = logical_address % max_segment_size
```

`max_segment_size` is received from Kernel Memory at startup. A read/write may
span more than one Memory Stick — the CPU splits the request per stick and
consolidates the result into a single operation. If `segment_offset + size`
exceeds the segment size, the Process is returned to the Kernel Scheduler to be
finished with **`SEG_FAULT`**.

## Configuration

| Key | Type | Description |
|-----|------|-------------|
| `LOG_LEVEL` | string | Maximum log detail (`log_level_from_string`). |
| `KERNEL_MEMORY_IP` / `KERNEL_MEMORY_PORT` | string / number | Kernel Memory endpoint. |
| `KERNEL_SCHEDULER_IP` / `KERNEL_SCHEDULER_PORT` | string / number | Kernel Scheduler endpoint. |

> Each CPU needs its own config file and log file. The consigna's example lists
> only `LOG_LEVEL`; the connection keys are project additions.

## Mandatory logs

Emitted at `INFO`. The consigna lists these in Spanish; this repository emits
them in English.

- Instruction fetch — `## PID: <PID> - FETCH - Program Counter: <PC>`
- Interrupt received — `## Interrupt received`
- Instruction executed — `## PID: <PID> - Running: <INSTR> - <ARGS>`
- Memory read/write — `PID: <PID> - Action: <READ/WRITE> - Physical Address: <ADDR> - Value: <VALUE>`

## Source layout

| File | Responsibility |
|------|----------------|
| `src/main.c` | Argument parsing, connection bootstrap, main loop. |
| `initializer.c` | Config load, logger, socket setup. |
| `connections.c` | Handshakes with scheduler, memory and sticks; Memory Stick discovery. |
| `cpu.c` | Instruction-cycle driver, interrupt checking. |
| `handlers.c` | One handler per instruction. |
| `registers.c` | Register get/set by name and width. |
| `memory.c` | MMU translation and Memory Stick read/write. |
| `cleanup.c` | Teardown. |
