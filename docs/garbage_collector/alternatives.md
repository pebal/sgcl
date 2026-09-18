# SGCL next to the alternatives

The collector against `shared_ptr`/`unique_ptr`, Go and Java with ZGC, in one table; the numbers are those of [the benchmarks](benchmarks.md).
| | SGCL | `shared_ptr` / `unique_ptr` | Go | Java, ZGC |
|---|---|---|---|---|
| Pauses, safepoints | none: no thread is ever stopped or asked to reach a point | none | short stop-the-world phases, preemption at safepoints | short pauses at phase changes, safepoints |
| What a mutator waits for | nothing in the collector: a page from the heap under a mutex (once per 64 KB), the memory ceiling | the destructor cascade of what it releases | allocation assists when the collector is behind | allocation stalls when the collector is behind |
| Pointer copy | a store and a byte of state, 1.4 ns onto the stack and 1.8 into an object (the card), the same when a thread copies a shared object's pointer | a reference count update, 5 ns alone and 100–300 ns on a shared object | a store, 0.6 ns, plus the barrier while marking | a store and a load barrier, 1 ns |
| Allocation | 4 ns, a per-thread bitmap, no lock | 21 ns, malloc | 7 ns, assists included | 3 ns, TLAB |
| Cycles | collected | leak unless broken by hand | collected | collected |
| Objects | never move | never move | never move | relocated, with load barriers |
| Heap | precise, through pointer maps the collector builds itself | | precise, stack maps from the compiler | precise |
| Stacks | conservative | | precise | precise |
| Destructors | deterministic through `unique_ptr`, otherwise on the collector's threads | deterministic | finalizers | cleaners |
| Weak pointers | `weak_ptr`, one word, cleared by the cycle that finds the object unreachable | `weak_ptr`, a second count | `weak.Pointer` | `WeakReference` |
| Lock-free structures | `concurrent_queue`, `concurrent_stack`, `concurrent_map`, `concurrent_set`, `concurrent_unordered_map`, `concurrent_unordered_set`, `copy_on_write`, `channel`: the textbook algorithms with no reclamation scheme | hazard pointers, epochs or counted pointers, by hand or from a library | channels (a lock and a buffer) and `sync.Map` (a hash map: reads from a snapshot, writes under a lock); anything lock-free by hand on `atomic.Pointer` | `java.util.concurrent` |
| Generations | young cycles with sticky marks and cards, full cycles on a schedule; full cycles only as an option | | none (a non-generational collector by design) | young and old, ZGC generational |
| Memory | a cycle every quarter of growth; no throttling, the memory grows when the program outruns the collector | exact | kept near `GOGC` by throttling the mutators | kept under the heap ceiling by stalling the mutators |
| Runtime | this header-only library | the standard library | the Go runtime | the JVM |
