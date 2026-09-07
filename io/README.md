# IO

Simulates the system's I/O interfaces. One process, one interface type, chosen at
startup: `STDIN`, `STDOUT` or `SLEEP`.

## Running

```sh
./bin/io [config file] [type]
```

`type` is `STDIN`, `STDOUT` or `SLEEP`. The module connects to the Kernel
Scheduler and waits for requests. It has a **single thread of execution**: it
handles one request at a time and answers when done.

## Behaviour by type

| Type | Behaviour |
|------|-----------|
| `STDIN` | Reads a given number of bytes from the keyboard (input ends on Enter). Longer input is truncated to the requested size; shorter input is padded with `\0`. The bytes are returned to the Kernel Scheduler. |
| `STDOUT` | Receives a byte string, prints it to screen and to the log file, then tells the Kernel Scheduler the IO finished. |
| `SLEEP` | Receives a time in milliseconds, runs `usleep` for that long, then tells the Kernel Scheduler the IO finished. |

## Configuration

| Key | Type | Description |
|-----|------|-------------|
| `LOG_LEVEL` | string | Maximum log detail (`log_level_from_string`). |
| `KERNEL_SCHEDULER_IP` / `KERNEL_SCHEDULER_PORT` | string / number | Kernel Scheduler endpoint. |

> The connection keys are project additions to the consigna's example.

## Mandatory logs

Emitted at `INFO`. The consigna lists these in Spanish; this repository emits
them in English.

- Connection to Kernel Scheduler — `## Connected to Kernel Scheduler`
- IO start — `## PID <PID> - IO start`
- IO end — `## PID <PID> - IO end`
- `STDOUT` only — `## PID: <PID> - <CONTENT>`
- `STDIN` only — `## PID <PID> - Enter <COUNT> characters`
- `SLEEP` only — `## PID: <PID> - Sleeping for <TIME> seconds`

## Source layout

| File | Responsibility |
|------|----------------|
| `src/main.c` | Argument parsing, connection, request loop. |
| `utils.c` | Config load, argument parsing, scheduler connection. |
| `ioops.c` | The `STDIN` / `STDOUT` / `SLEEP` request handlers. |

## Log inventory

Ordered by level, then by how often each line fires. `LOG_LEVEL` is the lowest
level that reaches the file (`INFO` shows `INFO`/`WARNING`/`ERROR`).

| Level | Frequency | Message | Where |
|-------|-----------|---------|-------|
| `INFO` | per request (mandatory) | `## PID <PID> - IO start` / `## PID <PID> - IO end` | `ioops.c` |
| `INFO` | per `STDOUT` request (mandatory) | `## PID: <PID> - <content>` | `ioops.c` |
| `INFO` | per `STDIN` request (mandatory) | `## PID <PID> - Enter <count> characters` | `ioops.c` |
| `INFO` | per `SLEEP` request (mandatory) | `## PID: <PID> - Sleeping for <time> seconds` | `ioops.c` |
| `INFO` | once, on connect (mandatory) | `## Connected to Kernel Scheduler` | `utils.c` |
| `INFO` | once, on shutdown | `## IO shutting down` | `main.c` |
| `WARNING` | on peer loss / end of input | `## Could not send the IO response to Kernel Scheduler (peer gone?)` / `## Could not read user input (end of input?)` | `ioops.c` |
| `WARNING` | rare (protocol desync) | `Unexpected operation <op> from Kernel Scheduler; shutting down` | `main.c` |
| `ERROR` | rare (startup failure) | `## Connection error with Kernel Scheduler` / `## Handshake error with Kernel Scheduler` / `## Error sending the IO type` | `utils.c` |
| `ERROR` | rare (bad request) | `## Malformed STDOUT request: no content` | `ioops.c` |
| `DEBUG` | once, on startup | `Config loaded: <type> interface, Kernel Scheduler at <ip>:<port>` | `utils.c` |
| `DEBUG` | once, on connect | `Handshake successful with Kernel Scheduler` / `IO type sent successfully` | `utils.c` |
| `TRACE` | per request | `Received operation <op> from Kernel Scheduler` | `main.c` |
