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

- Connection to Kernel Memory — `## Connected to Kernel Memory`
- Each CPU connection
- Process creation — `## <PID> Creating the process - State: NEW`
- Syscall received — `## <PID> - Requested syscall: <NAME>`
- State change — `## <PID> moves from state <FROM> to state <TO>`
- IO end — `##  <<PID>> - Finished IO and moves to READY / SUSP. READY`
- Mutex taken / released — `## <PID> Takes the Mutex <NAME>` / `## <PID> Releases the Mutex <NAME>`
- Priority change — `## <PID> Change of priority: <OLD> - <NEW>`
- Preemption by quantum end — `## <PID> - Preempted due to quantum end`
- Preemption by a higher-priority queue — `## <PID> Priority: <P> - Preempted by a higher-priority queue by process <PID> with priority <P>`
- Compaction start / end — `## Start of compaction` / `## End of compaction`
- Process end — `## <PID> finished execution with reason: <REASON>`

## Source layout

| File | Responsibility |
|------|----------------|
| `src/main.c` | Argument parsing, bootstrap, shutdown. |
| `kernel_scheduler.c` | Config loading, algorithm parsing. |
| `server.c` | Multithreaded listener for CPUs and IO interfaces. |
| `cpu.c` | Per-CPU thread: syscall dispatch, preemption, interrupts. |
| `io.c` | Per-IO-interface request queues and `BLOCK` handling. |
| `queue.c` | The seven-state machine; long/medium/short-term transitions; suspension and compaction routines. |
| `mutex.c` | Mutex ownership, wait queues, priority inheritance. |
| `memory.c` | `MEM_ALLOC` / `MEM_FREE` requests and compaction coordination with Kernel Memory. |
| `misc.c` | Process/thread counters, shutdown reasons, shared helpers. |
