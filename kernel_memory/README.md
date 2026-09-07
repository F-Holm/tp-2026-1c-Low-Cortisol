# Kernel Memory

Manages all of the system's memory: the execution context and instructions of
every Process, the user memory spread across the connected **Memory Sticks**, and
the **SWAP** space used for suspended Processes.

## Running

```sh
./bin/kernel_memory [config file]
```

It is the first module to start. On startup it opens a multithreaded server that
concurrently serves the Kernel Scheduler and the CPUs, and also accepts the
connections of the Memory Sticks and the SWAP module. CPUs and Memory Sticks may
connect throughout the run, so the listener stays open.

## Instruction and context memory

- **Execution context** — per PID: a full copy of the CPU registers plus the
  Process's segment table. Sent to / received from a CPU on request.
- **Pseudocode files** — per PID: a text file (one instruction per line) located
  relative to `SCRIPTS_BASEPATH`. Kernel Memory serves the instruction requested
  by a CPU's program counter.

## User memory

Pure **segmentation**: per PID a table of segments, each with a base and a
limit. Storage lives in the Memory Sticks, which report their size when they
connect and are appended to the stick list, growing total memory. When a new
stick enlarges total memory, the Kernel Scheduler is notified that more memory is
available. If a stick disconnects mid-run, the Kernel Scheduler is notified that
memory is **corrupted**.

## Operations

| Operation | Description |
|-----------|-------------|
| **Create Process** | Receives a PID and a pseudocode path; builds the execution context with all registers at 0. |
| **Create Segment** | Receives PID, segment id and size; places it in a free hole chosen by `ALLOCATION_STRATEGY` (`BEST` or `WORST` fit). |
| **Compaction** | When enough free space exists but is not contiguous, asks the Kernel Scheduler to preempt every CPU, then packs all segments to the start of memory (segments may move partially or fully between sticks) and updates every Process's segment table. |
| **Delete Segment** | Receives PID and segment id; removes the table entry and marks the space free. |
| **Suspend Process** | Moves every segment to SWAP blocks one at a time, freeing memory as each copy completes. SWAP blocks are assigned per segment — a block never holds two segments. |
| **Resume Process** | Restores every segment from SWAP to main memory, regenerating the segment table using the hole-selection algorithm. |
| **Terminate Process** | Receives a PID; frees every segment and structure associated with it. |
| **Read data** | Serves a physical address (or logical address, or a list) and a size; returns the bytes, split across Memory Sticks as needed and consolidated into one result. |
| **Write data** | Serves a physical address and bytes; writes them across the involved Memory Sticks. |

## Configuration

| Key | Type | Description |
|-----|------|-------------|
| `LOG_LEVEL` | string | Maximum log detail (`log_level_from_string`). |
| `SEGMENT_MAX_SIZE` | number | Largest size any single segment may have. |
| `ALLOCATION_STRATEGY` | string | Hole-selection algorithm: `BEST` or `WORST`. |
| `INSTRUCTION_DELAY` | number | ms to wait before answering an instruction request. |
| `COMPACTION_DELAY` | number | ms to wait before declaring a compaction finished. |
| `SCRIPTS_BASEPATH` | string | Directory holding the pseudocode files. |
| `KERNEL_MEMORY_IP` / `KERNEL_MEMORY_PORT` | string / number | Endpoint this module listens on. |

> The consigna's example uses `KERNEL_MEMORY_PUERTO`; this repository uses
> `KERNEL_MEMORY_PORT` (and `KERNEL_MEMORY_IP`).

## Mandatory logs

Emitted at `INFO`. The consigna lists these in Spanish; this repository emits
them in English.

- CPU connection — `## CPU <ID> connected`
- Memory Stick connection — `## Memory Stick of <SIZE> bytes connected`
- Kernel Scheduler connection — `## Kernel Scheduler Connected - FD of the socket: <FD>`
- Process creation — `## PID: <PID>  - Process created`
- Get instruction — `## PID: <PID> - Get instruction: <PC> - Instruction: <INSTR> <ARGS>`
- Segment creation — `## PID: <PID> - Segment created <ID> - Size: <SIZE>`
- User-space read / write — `## PID: <PID> - <Read/Write> - Phys. Addr: <ADDR> - Size: <SIZE>`

## Source layout

| File | Responsibility |
|------|----------------|
| `src/main.c` | Argument check, bootstrap. |
| `configurator.c` | Config loading and validation. |
| `server.c` | Multithreaded server; handshakes for scheduler, CPUs, sticks, SWAP. |
| `listeners.c` | Per-connection request loops (scheduler, CPU, SWAP). |
| `protocol.c` | Segment placement, hole selection, compaction, address translation, stick read/write. |
| `swap.c` | Process suspension / resume against the SWAP module. |
| `initializer.c` | Context creation, main-memory and hole-list setup, stick IP resolution. |
| `cleanup.c` | Teardown of every per-connection and global structure. |
| `structs.h` | Shared data types. |

## Log inventory

Ordered by level, then by how often each line fires. `LOG_LEVEL` is the lowest
level that reaches the file (`INFO` shows `INFO`/`WARNING`/`ERROR`).

| Level | Frequency | Message | Where |
|-------|-----------|---------|-------|
| `INFO` | per instruction request (mandatory) | `## PID: <PID> - Get instruction: <PC> - Instruction: <instr>` | `listeners.c` |
| `INFO` | per user read / write (mandatory) | `## PID: <PID> - <Read/Write> - Phys. Addr: <addr> - Size: <size>` | `listeners.c` |
| `INFO` | per segment create / regenerate (mandatory) | `## PID: <PID> - Segment created/regenerated <id> - Size: <size>` | `protocol.c`, `swap.c` |
| `INFO` | per process create (mandatory) | `## PID: <PID>  - Process created` | `listeners.c` |
| `INFO` | per CPU / stick connect (mandatory) | `## CPU <id> connected` / `## Memory Stick of <size> bytes connected` | `protocol.c` |
| `INFO` | once, scheduler connect (mandatory) | `## Kernel Scheduler Connected - FD of the socket: <fd>` | `server.c` |
| `INFO` | per process end / suspend / resume | `Process with PID <PID> ended`, `Process PID <PID> was suspended/resumed successfully` | `listeners.c`, `swap.c` |
| `INFO` | once, on startup | `## Kernel Memory started` | `src/main.c` |
| `WARNING` | on stick send / response failure (→ BSOD) | `Error sending read/write packet to stick <i>`, `Wrong response opcode from stick <i>`, `No more sticks available ...`, `Error writing to sticks`, `Notifying the Kernel Scheduler that memory is corrupted` | `protocol.c`, `listeners.c` |
| `WARNING` | on swap read/write anomaly | `Unexpected response from the swap module ...`, `Could not read block <n> from swap ...`, `## Could not allocate any hole.` | `swap.c` |
| `ERROR` | rare (bad data / logic) | `Process with pid <n> not found`, `Segment not found`, `Segment number <n> was not found ...`, `The chosen hole-selection option is not valid.`, `The holes adjacent to the segment were not found`, `Error: unrecognized operation code`, `The computed physical address does not match any connected stick` | `listeners.c`, `protocol.c` |
| `ERROR` | rare (startup / handshake) | `Error sending handshake for: <s>`, `Closed the connection with <s> ...`, `## PID: <PID> - Could not open the file: <p>`, `## Could not resolve the memory stick IP`, `Unexpected opcode while receiving swap info` | `error.c`, `initializer.c` |
| `DEBUG` | per request / soft failure | `Received a MEM_ALLOC/MEM_FREE/SUSPEND/RESUME request`, `Not enough space to create/regenerate the segment`, `No available holes`, `no free blocks`, `... was not found`, connect confirmations | `listeners.c`, `protocol.c`, `swap.c`, `server.c` |
| `TRACE` | per operation internals | `Calculating holes...`, `free space computed/sent`, `iterating segment list ...`, `Segment between/after/before hole`, `Reading/Read <n> bytes from physical_address ...`, `Get registers`, `Sending/Segment table sent`, `Adding/Removing swap block ...` | `protocol.c`, `listeners.c`, `swap.c` |
