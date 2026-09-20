# SGCL documentation

The guide, the rules and the benchmarks are in the [main README](../README.md). This directory is the reference, a page per class, and the pages about the engine.

| where | what |
|---|---|
| [sgcl/](sgcl/README.md) | the `sgcl` namespace: the interface of the standard library in snake_case. A page per class or function (`tracked_ptr`, `make_tracked`, the containers, `atomic`, `channel`, `task`, `collector`, ...), every public member with its signature and an example that compiles. `#include "sgcl/sgcl.h"` |
| [garbage_collector/](garbage_collector/README.md) | the engine: the engine in short, next to the alternatives, the benchmarks, how it works (the heap, a slot's states, the barrier, the roots, a cycle phase by phase, young and full cycles, the weak phase, allocation), the diagnostics (what is alive and why, what a rule broken looks like, what a cycle costs, a race with the collector; the debugger); the constants that tune it are on [config](sgcl/core/config.md) in `core` |

The reference lies in one directory per module, the layout of the headers (`sgcl/{core,containers,concurrent,async,io}/`), the page of a class next to the pages of its module, a `README.md` in every module directory that describes the module and lists its classes ([sgcl/core](sgcl/core/README.md), [containers](sgcl/containers/README.md), [concurrent](sgcl/concurrent/README.md), [async](sgcl/async/README.md), [io](sgcl/io/README.md)), and the index of the whole in [sgcl/README.md](sgcl/README.md).

