# SWAP

Stores the memory of Processes that the Kernel Scheduler suspends, when Kernel
Memory tells it to. It is a dumb block store: all bookkeeping lives in Kernel
Memory.

## Running

```sh
./bin/swap [config file]
```

The module keeps a single file (`SWAP_FILE_PATH`, `SWAP_FILE_SIZE` bytes) divided
into fixed-size blocks (`BLOCK_SIZE`). The file is assumed to start empty and is
not cleared explicitly. On startup it reports the block size and total size to
Kernel Memory.

## Operations

Both operations always act on exactly one block.

| Operation | Input | Result |
|-----------|-------|--------|
| **Write block** | block number + content | confirmation to Kernel Memory |
| **Read block**  | block number | the bytes read |

## Configuration

| Key | Type | Description |
|-----|------|-------------|
| `LOG_LEVEL` | string | Maximum log detail (`log_level_from_string`). |
| `SWAP_FILE_PATH` | string | Path to the SWAP file. |
| `SWAP_FILE_SIZE` | number | Total size of the SWAP file, in bytes. |
| `BLOCK_SIZE` | number | Size of each block, in bytes. |
| `KERNEL_MEMORY_IP` / `KERNEL_MEMORY_PORT` | string / number | Kernel Memory endpoint. |

> The IP/port keys are project additions to the consigna's example.

## Mandatory logs

Emitted at `INFO`. The consigna lists these in Spanish; this repository emits
them in English.

- Connection to Kernel Memory — `## Connected to Kernel Memory`
- Write — `## Block write: <NUMBER>`
- Read — `## Block read: <NUMBER>`

## Source layout

| File | Responsibility |
|------|----------------|
| `src/main.c` | Argument check, bootstrap, request loop. |
| `swap.c` | Config load, SWAP-file setup, per-block read/write. |
