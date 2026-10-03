# SGCL documentation

The guide, the rules and the benchmarks are in the [main README](../README.md). This directory is the reference, a page per class, and the pages about the engine.

| Where | What |
|---|---|
| [sgcl/](sgcl/README.md) | the reference of the library: the list of the modules, a README per module, a page per class and per public method, every example a program that compiles |
| [garbage_collector/](garbage_collector/README.md) | the engine: the engine in short, next to the alternatives, the benchmarks, how it works (the heap, a slot's states, the barrier, the roots, a cycle phase by phase, young and full cycles, the weak phase, allocation), the diagnostics (what is alive and why, what a rule broken looks like, what a cycle costs, a race with the collector; the debugger); the constants that tune it are on [config](sgcl/core/config.md) in `core` |
| [STYLE.md](STYLE.md) | how a page of the documentation is written |
