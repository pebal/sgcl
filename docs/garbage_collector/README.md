# The garbage collector

The pages about the engine under both interfaces: how the collector works, how it stands next to the alternatives and in the benchmarks, and what it gives a program to see what it is doing. The interface of the collector itself (a forced cycle, statistics, the memory limit) is a class of the `core` module, [`collector`](../sgcl/core/collector.md) in `sgcl` and [`Collector`](../sgcl/Sgcl/Core/Collector.md) in `Sgcl`; the constants that size and tune it are on [config](../sgcl/core/config.md), a header of the same module; the code of the engine is in `sgcl/core/detail/`.

| page | what it is |
|---|---|
| [the engine in short](overview.md) | the collector in a page: the mechanism, the pointer maps, the stack roots, the generations, the memory |
| [next to the alternatives](alternatives.md) | the collector against `shared_ptr`/`unique_ptr`, Go and Java with ZGC, in one table |
| [benchmarks](benchmarks.md) | the setup and the numbers: allocation, a pointer copy, binary-trees, a shared graph, a large live tree, and what the tables say and do not; the benchmarks of the containers and of the lock-free containers are with their modules |
| [how it works](how-it-works.md) | the engine: the heap, a slot's states, the barrier, the roots, a cycle phase by phase, epochs and parity, young and full cycles, marking in parallel, the weak phase, the cells of the `root_ptr`s and when to use which pointer, allocation, what makes it pause-free |
| [diagnostics](diagnostics.md) | the tools and the cases: what is alive and why, what a rule broken looks like, what a cycle costs, a race with the collector; the debugger |
