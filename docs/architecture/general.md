# Architecture

## Layered Design

The library is organized into five layers, each depending only on the layers below:

```
Core → Containers/Mem Lower → Log → Mem Upper
```

### Core
Foundational data structures and algorithms.

### Containers
[Data structures **without allocators**](containers.md)

### Mem
Pure memory management logic. Size calculations, alignment, pool metadata, reserve tracking. **No syscalls, no logging.** Depends on Core only.

### Log
Observability layer. Traces operations, records metrics, and exports state from both Containers and Mem Lower. Depends on Core, Containers, and Mem Lower.

### Alloc
Orchestration layer. Handles `commit`/`reserve`/flag management and **makes the actual syscalls** (e.g., `mmap`, `VirtualAlloc`). Depends on Log and Mem Lower.

**Each layer depends only on the layers below it — never the reverse.**
