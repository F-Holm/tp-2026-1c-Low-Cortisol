# Memory Stick

Represents one memory chip: a flat address space that serves read and write
requests from the CPUs and from Kernel Memory.

## Running

```sh
./bin/memory_stick [config file] [size]
```

`size` is the number of bytes this stick provides. On startup it `calloc`s that
much memory, connects to Kernel Memory (reporting its size, which Kernel Memory
appends to the global memory), and then waits for CPU connections that will issue
read and write requests. Sticks can be started and stopped mid-run; unplugging
one makes Kernel Memory declare memory corrupted.

## Operations

| Operation | Input | Result |
|-----------|-------|--------|
| **Write** | physical address + content | confirmation to the caller |
| **Read**  | physical address + size | the bytes read |

Both wait `MEMORY_DELAY` ms before answering.

## Configuration

| Key | Type | Description |
|-----|------|-------------|
| `LOG_LEVEL` | string | Maximum log detail (`log_level_from_string`). |
| `MEMORY_DELAY` | number | ms to wait before answering a read or write. |
| `KERNEL_MEMORY_IP` / `KERNEL_MEMORY_PORT` | string / number | Kernel Memory endpoint. |

> The IP/port keys are project additions to the consigna's example.

## Mandatory logs

Emitted at `INFO`. The consigna lists these in Spanish; this repository emits
them in English.

- Connection to Kernel Memory — `## Connected to Kernel Memory`
- CPU connection — `## CPU <ID> connected`
- Write — `## Write of <COUNT> bytes`
- Read — `## Read of <COUNT> bytes`

## Source layout

| File | Responsibility |
|------|----------------|
| `src/main.c` | Argument parsing, bootstrap, request loop. |
| `memory_stick.c` | Config load, memory allocation, read/write against the buffer. |
| `kernel_memory.c` | Handshake with Kernel Memory and size reporting. |
| `cpu.c` | CPU server: accepts CPU connections and dispatches their requests. |
