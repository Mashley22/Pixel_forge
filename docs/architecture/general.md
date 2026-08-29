# Architecture

## Layered Design

The library is organized into five layers, each depending only on the layers below:

```
Core → Adapters/Mem Lower → Log → Mem Upper
```

### Core
Foundational data structures and algorithms.

### Adapters
[Data structures **without allocators**](adapters.md)

### Mem
Pure memory management logic. Size calculations, alignment, pool metadata, reserve tracking. **No syscalls, no logging.** Depends on Core only.

### Log
Observability layer. Traces operations, records metrics, and exports state from both Adapters and Mem Lower. Depends on Core, Adapters, and Mem Lower.

### Alloc
Orchestration layer. Handles `commit`/`reserve`/flag management and **makes the actual syscalls** (e.g., `mmap`, `VirtualAlloc`). Depends on Log and Mem Lower.

**Each layer depends only on the layers below it — never the reverse.**
