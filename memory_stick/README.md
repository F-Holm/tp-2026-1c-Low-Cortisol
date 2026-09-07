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

## Log inventory

Ordered by level, then by how often each line fires. `LOG_LEVEL` is the lowest
level that reaches the file (`INFO` shows `INFO`/`WARNING`/`ERROR`).

| Level | Frequency | Message | Where |
|-------|-----------|---------|-------|
| `INFO` | per read (mandatory) | `## Read of <count> bytes` | `main.c`, `cpu.c` |
| `INFO` | per write (mandatory) | `## Write of <count> bytes` | `main.c`, `cpu.c` |
| `INFO` | per CPU connection (mandatory) | `## CPU <id> connected` | `cpu.c` |
| `INFO` | once, on connect (mandatory) | `## Connected to Kernel Memory` | `kernel_memory.c` |
| `INFO` | once, on shutdown | `## Memory Stick shutting down` | `main.c` |
| `WARNING` | on CPU handshake / request failure | `## Could not receive/send the handshake ...`, `## Could not receive the CPU ID` | `cpu.c` |
| `WARNING` | rare (protocol desync) | `Unexpected operation <op> from Kernel Memory; shutting down` | `main.c` |
| `ERROR` | rare (startup failure) | `## Connection error with Kernel Memory`, `## Could not send/receive the handshake to Kernel Memory`, `## Could not send the size to Kernel Memory`, `## Error sending the CPU server port`, `## Error creating the CPU server`, `## Could not create the CPU server thread` | `kernel_memory.c`, `memory_stick.c`, `cpu.c` |
| `ERROR` | rare (bad request) | `## Invalid read/write request from Kernel Memory` / `... from the CPU` | `main.c`, `cpu.c` |
| `DEBUG` | once per connection | `Handshake successful with Kernel Memory`, `Size reported to Kernel Memory`, `CPU server created successfully`, `CPU server port sent successfully`, `Handshake successful with the CPU`, `Connection established with a CPU` | `kernel_memory.c`, `memory_stick.c`, `cpu.c` |
| `TRACE` | per request | `Receiving a read/write instruction ...`, `Read/Write from Kernel Memory of <n> bytes ...` | `main.c`, `cpu.c` |
| `TRACE` | per memory access | `Reading <n> bytes from offset <p>`, `Read <n> bytes`, `Wrote <n> bytes` | `memory_stick.c` |
