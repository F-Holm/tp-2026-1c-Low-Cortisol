# Kernel Scheduler

Schedules the simulated **Processes** across a seven-state model
(`NEW · READY · EXEC · BLOCK · SUSP. READY · SUSP. BLOCK · EXIT`). It is the
module CPUs and IO interfaces talk to, and it drives every state transition a
Process goes through.

## Running

```sh
./bin/kernel_scheduler [config file] [initial process path]
```

Kernel Memory must already be running. The second argument is the pseudocode
file for the **initial Process** (PID 0), which always has the maximum priority
and is the root from which every other Process is spawned via `INIT_PROC`.

On startup the module connects to Kernel Memory and then opens a multithreaded
server that concurrently serves CPU and IO connections. CPUs may connect and
disconnect at any time, so the listener stays open for the whole run.

## Responsibilities

### Long-term scheduling (NEW → READY)
Processes start with no memory assigned, so this transition has no admission
limit. If Kernel Memory reports memory corruption (a Memory Stick was
unplugged), every Process is terminated and the module shuts down with a
**BSOD** reason.

### Medium-term scheduling (BLOCK ↔ SUSP. BLOCK, SUSP. READY → READY)
Each time a Process enters `BLOCK`, a timer of `SUSPENSION_TIMEOUT` ms starts; if
it is still blocked when the timer expires it is moved to `SUSP. BLOCK` (its
memory is swapped out by Kernel Memory). When memory frees up — a segment is
released, a Memory Stick is added, or a compaction finishes — suspended
Processes are walked in priority order (ties broken by longest time suspended)
and resumed if they fit without triggering a compaction.

### Short-term scheduling (READY ↔ EXEC)
One algorithm per run, chosen by config:

| Algorithm | Behaviour |
|-----------|-----------|
| `FIFO` | First-in first-out, priority ignored. |
| `RR`   | Round-robin with `RR_QUANTUM` ms, priority ignored. |
| `CMN`  | Multilevel queues (no feedback): one queue per priority level `0..N-1`, each running `FIFO` or `RR` as listed in `QUEUE_ALGORITHMS`. With `QUEUE_PREEMPTION=TRUE`, an arriving Process preempts a lower-priority running one. |

Processes with a priority outside the configured range are never scheduled.

### Syscall handling
- **IO** (`SLEEP`, `STDIN`, `STDOUT`): the Process moves `EXEC → BLOCK` and the
  freed CPU gets another Process. `STDIN` forwards the characters read by the IO
  module to Kernel Memory; `STDOUT` reads the bytes from Kernel Memory and sends
  them to the IO module.
- **Memory** (`MEM_ALLOC`, `MEM_FREE`): once resolved, the same Process is sent
  back to the CPU that made the call. A segment creation that needs a compaction
  triggers the flow below.
- **Mutex** (`MUTEX_CREATE`, `MUTEX_LOCK`, `MUTEX_UNLOCK`): see below.

### Compaction preemption
When Kernel Memory asks for a compaction, the scheduler tells every CPU to
preempt its Process and dispatches nothing more until Kernel Memory confirms the
compaction is done. Preempted Processes are placed at the **front** of `READY`,
then everything is rescheduled.

### Mutexes and priority inheritance
A mutex is held by at most one Process; waiters are granted it in the order they
requested it. If a low-priority holder blocks a higher-priority Process, the
holder **temporarily inherits** the highest priority among the Processes waiting
on that mutex, reverting to its own priority on unlock.

## Configuration

| Key | Type | Description |
|-----|------|-------------|
| `LOG_LEVEL` | string | Maximum log detail (`log_level_from_string`). |
| `SCHEDULING_ALGORITHM` | string | `FIFO`, `RR` or `CMN`. |
| `QUEUE_ALGORITHMS` | list | Per-level algorithm list, used when `CMN`, e.g. `[FIFO,RR,RR,FIFO]`. |
| `RR_QUANTUM` | number | Round-robin quantum in ms. |
| `QUEUE_PREEMPTION` | string | `TRUE` / `FALSE`, inter-queue preemption. |
| `SUSPENSION_TIMEOUT` | number | ms in `BLOCK` before a Process is suspended. |
| `KERNEL_MEMORY_IP` / `KERNEL_MEMORY_PORT` | string / number | Kernel Memory endpoint. |
| `KERNEL_SCHEDULER_PORT` | number | Port this module listens on. |

> The consigna names these keys `PLANIFICATION_ALGORITHM` and
> `QUEUES_ALGORITHMS`; this repository renamed them to `SCHEDULING_ALGORITHM` and
> `QUEUE_ALGORITHMS`, and the IP/port keys are project additions.

## Mandatory logs

Emitted at `INFO`. The consigna lists these in Spanish; this repository emits
them in English.

- Connection to Kernel Memory — `Connected to Kernel Memory`
- Each CPU connection
- Process creation — `<PID> Creating the process - State: NEW`
- Syscall received — `<PID> - Requested syscall: <NAME>`
- State change — `<PID> moves from state <FROM> to state <TO>`
- IO end — `<PID> - Finished IO and moves to READY / SUSP. READY`
- Mutex taken / released — `<PID> Takes the Mutex <NAME>` / `<PID> Releases the Mutex <NAME>`
- Priority change — `<PID> Change of priority: <OLD> - <NEW>`
- Preemption by quantum end — `<PID> - Preempted due to quantum end`
- Preemption by a higher-priority queue — `<PID> Priority: <P> - Preempted by a higher-priority queue by process <PID> with priority <P>`
- Compaction start / end — `Start of compaction` / `End of compaction`
- Process end — `<PID> finished execution with reason: <REASON>`

## Source layout

Sources live under `src/kernel_scheduler/` in packages:

### `domain/` — core entities, no I/O
| File | Responsibility |
|------|----------------|
| `pcb.c` | The `t_pcb` process control block: creation, teardown, state/priority accessors, active-instance counting. |
| `kernel_memory_socket.c` | The `t_kernel_memory_socket` wrapper that serialises the connection to Kernel Memory. |

### `scheduler/` — the queues and the state machine
| File | Responsibility |
|------|----------------|
| `queue_types.h` | Every scheduler struct, including the `t_queues` aggregate. |
| `queues.c` | The seven-state machine and the suspension/resumption worker threads. |
| `queues.h` | Umbrella header re-exporting the package. |
| `compaction.c` | The memory-compaction and resumption-sweep routines (detached threads). |
| `ready_queue.c` | `READY` as a data structure (single or multilevel), the preempt/terminate gates. |
| `exec_list.c` | `EXEC` as a data structure and the "exec drained" waits. |
| `blocking_list.c` | `BLOCK` / `SUSP. BLOCK` / `SUSP. READY` as data structures. |
| `counter.c` | The `t_counter` primitive (thread and syscall counters). |
| `memory_query.c` | Free-space / process-size request-response exchanges with Kernel Memory. |
| `process_counter.c` | Live-process count; triggers `SR_NO_PROCESSES` shutdown when it hits zero. |

### `connections/` — everything socket-facing
| File | Responsibility |
|------|----------------|
| `server.c` | Multithreaded listener for CPUs and IO interfaces. |
| `cpu.c` | Per-CPU thread: syscall dispatch, preemption, interrupts. |
| `io.c` | Per-IO-interface request queues and `BLOCK` handling. |
| `kernel_memory.c` | The connection to Kernel Memory: connect, handshake, the connection-check watchdog. |

### `syscalls/` — syscall implementations
| File | Responsibility |
|------|----------------|
| `mutex.c` | Mutex ownership, wait queues, priority inheritance. |
| `memory.c` | `MEM_ALLOC` / `MEM_FREE` requests and compaction coordination with Kernel Memory. |

### `app/` and top level
| File | Responsibility |
|------|----------------|
| `../main.c` | Argument parsing, bootstrap, shutdown. |
| `app/kernel_scheduler.c` | Config loading, algorithm parsing, module start/close. |
| `shutdown.c` | The idempotent `close_kernel_scheduler` routine and the shutdown-reason resolution. |
| `common/time.c` | `millis` / `time_diff`. |
| `common/handshake.c` | `respond_handshake` (the module's handshake reply). |

## Log inventory

Ordered by level, then by how often each line fires. `LOG_LEVEL` is the lowest
level that reaches the file (`INFO` shows `INFO`/`WARNING`/`ERROR`).

| Level | Frequency | Message | Where |
|-------|-----------|---------|-------|
| `INFO` | per state change (mandatory) | `<PID> moves from state <from> to state <to>` | `scheduler/queues.c` |
| `INFO` | per syscall (mandatory) | `<PID> - Requested syscall: <name>` | `connections/cpu.c` |
| `INFO` | per process create / end (mandatory) | `<PID> Creating the process - State: NEW` / `<PID> finished execution with reason: <reason>` | `scheduler/queues.c` |
| `INFO` | per mutex op (mandatory) | `<PID> Takes/Releases the Mutex <name>` | `syscalls/mutex.c` |
| `INFO` | per priority change (mandatory) | `<PID> Change of priority: <old> - <new>` | `syscalls/mutex.c` |
| `INFO` | per preemption (mandatory) | `<PID> - Preempted due to quantum end`, `<PID> Priority: <p> - Preempted by a higher-priority queue ...` | `connections/cpu.c` |
| `INFO` | per IO end (mandatory) | `<PID> - Finished IO and moves to READY / SUSP. READY` | `connections/io.c` |
| `INFO` | per compaction (mandatory) | `Start of compaction` / `End of compaction` | `scheduler/queues.c` |
| `INFO` | once, on connect (mandatory) | `Connected to Kernel Memory` | `connections/kernel_memory.c` |
| `INFO` | per CPU / IO connection | `CPU <id> connected`, `IO of type <t> connected` | `connections/cpu.c`, `connections/io.c` |
| `INFO` | once, on shutdown | `<reason>` (from `SHUTDOWN_REASONS`) | `shutdown.c` |
| `WARNING` | on peer loss (CPU / IO / KM) | `Error communicating with Kernel Memory`, `Error sending to IO`, `CPU <id>: could not send the interrupt decision`, `The ... request queue was empty`, `IO operation of type <t> failed`, `Priority preemption failed` | `connections/io.c`, `connections/cpu.c`, `syscalls/memory.c` |
| `WARNING` | on bad handshake / duplicate | `Invalid handshake received`, `Duplicate IO type ...` | `connections/server.c`, `connections/io.c` |
| `ERROR` | rare (thread / server creation) | `Error creating the CPU/suspender/resumer/... thread`, `Error creating the server` | `connections/cpu.c`, `scheduler/queues.c`, `connections/server.c` |
| `ERROR` | rare (state-machine violation / bad data) | `<PID> Cannot move from state ...`, `Wrong operation type. Expected: OP_IO_TYPE`, `Invalid IO type: <s>`, `Error sending the handshake to <s>`, `<reason>` (error `SHUTDOWN_REASONS`) | `scheduler/queues.c`, `connections/io.c`, `shutdown.c`, `common/handshake.c` |
| `DEBUG` | per dispatch / soft outcome | `CPU %s: got process`, `Not enough space`, `Could not suspend/resume process <PID>`, `... thread started successfully`, connect/close confirmations | `connections/cpu.c`, `scheduler/queues.c`, `syscalls/memory.c`, `connections/server.c` |
| `TRACE` | per cycle / per routine tick | `CPU %s: CPU cycle OK`, `CPU %s: Operation received`, `CPU %s: Code sent successfully`, `Space available`, `Process size`, `Suspended threads locked/unlocked` | `connections/cpu.c`, `scheduler/queues.c` |
