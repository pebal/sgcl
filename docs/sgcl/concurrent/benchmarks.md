# Benchmarks: the lock-free containers

The setup, the machine, the environments and how the timers and the memory are read are described with [the benchmarks of the engine](../../garbage_collector/benchmarks.md); the numbers here were taken the same way, and retaken whole on 18 September 2026, after the reviews of the concurrent and async modules, with `benchmarks/compare.sh` (the best of three runs of every cell, the machine under its desktop load); where a cell moved for a reason, the reason is with it. The rows at 32 and 64 threads were added later, from one run of the same script on the same machine, which has 24 cores (16 performance and 8 efficiency): those two counts oversubscribe it, the case a lock-free structure exists for, since no thread there waits for a thread that was descheduled holding a word. The channel, a class of the [async](../async/README.md) module, is measured with the containers it is built on, at the end of the second section (the rest of that module, a hop, a wait and a race on the scheduler, is on [its own page](../async/benchmarks.md)); the single-producer queue and the cache, which have no counterpart in Go's or Java's library, close the page against what C++ has.

## Lock-free stack
A Treiber stack (the shape of `examples/lock_free_stack.cpp`): a compare-exchange on the head, with a collector free of ABA because a node is never reused while a thread holds it, and with the backoff of Herlihy and Shavit after a lost exchange, a wait that doubles up to `config::BackoffMax` pause instructions, some 40 µs on any platform (`sgcl/concurrent/detail/backoff.h`). Go's variant uses `atomic.Pointer`, Java's `AtomicReference`, each with the same backoff and the same pause: Go has no pause intrinsic, so the wait is an `isb` from an assembly stub (what Go's own runtime spins on), and Java's `Thread.onSpinWait()` is run as `isb` with `-XX:OnSpinWaitInst=isb` (HotSpot's default on arm64 is `yield`, a no-op on Apple silicon). The `shared_ptr` variant uses the standard library's atomic operations on a `shared_ptr` (`std::atomic_load`, `std::atomic_compare_exchange_weak`), which it implements with a lock, with the backoff; the `unique_ptr` variant, where every node has one owner, is a stack under a mutex, the classic answer without a collector. Every thread pushes a node and pops one, a million times; nanoseconds per push or pop:

| threads | SGCL | `unique_ptr` | `shared_ptr` | Go | Java ZGC |
|---|---|---|---|---|---|
| 1 | 9.9 | 18.1 | 39.0 | 10.8 | 12.8 |
| 4 | 9.8 | 50.7 | 120.8 | 11.6 | 28.5 |
| 16 | 10.8 | 38.4 | 113.7 | 18.8 | 88.6 |
| 32 | 12.4 | 42.2 | 119.9 | 66.8 | 103.2 |
| 64 | 13.2 | 38.8 | 118.5 | 67.2 | 113.5 |

The stack costs SGCL the same 9 to 11 ns on one thread and on sixteen: without the backoff sixteen threads at one word spend their time on lost exchanges (607 ns per operation in the previous version of this table, Go and Java the same), and the backoff turns that storm into near-serial exchanges, which is what a stack of one word can be at best; the mutex, which serializes the threads too, is at 38 ns, and the `shared_ptr` variant at 114, its lock inside every load and its count on every node. With the same pause Go is at 12 and 19 ns at four and sixteen threads, Java at 29 and 89: the backoff is the whole story of this table, and what is left between the three collectors at sixteen threads is how often an exchange is still lost (Java's `compareAndSet` goes through its barriers between the load and the exchange, a longer window). On one thread SGCL is the fastest of the three collectors (9.9 ns against Go's 10.8 and Java's 12.8): an operation costs a few `tracked_ptr` temporaries (the loaded head, the arguments of the compare-exchange, taken by value so that the target is held for the length of the call, as `sgcl::atomic<shared_ptr>` does), each a write barrier with its card, and a hazard pointer on every load. In a second mode, "pairs" (half the threads push a million nodes each, the other half pop them), the `shared_ptr` variant does not survive sixteen threads: a consumer descheduled while holding a popped node keeps every node popped after it alive through the `next` links, and releases the whole chain at once, recursively, off the end of its stack. A collector has no such chain to release. The `sgcl::` stack (its head a `sgcl::atomic`, its nodes linked by `sgcl::tracked_ptr`) pays the location check on the `sgcl::tracked_ptr`s the operation builds, the loaded head and the new node; the arguments of the compare-exchange are `sgcl::tracked_ptr`s inside the atomic, copied from the word a `sgcl::tracked_ptr` holds without a check: 3 ns per operation at any count.

## Concurrent containers
The lock-free containers ([Lock-free containers](README.md#lock-free-containers)) shared by every thread, holding pointers to objects of one `long` (the sets the `long`s themselves; `benchmarks/concurrent/concurrent.cpp`, `benchmarks/go/concurrent`, `benchmarks/java/Concurrent.java`). Java's are the structures the library's are modelled on, `ConcurrentLinkedQueue`, `ConcurrentLinkedDeque` used at one end, `ConcurrentSkipListMap`, `ConcurrentSkipListSet` and `ConcurrentHashMap`; Go has one of them in its library, `sync.Map` (a hash map with reads from a snapshot and writes under a lock, its keys boxed in `any`), so that is its hash-map column, and the others are the same algorithms written on `atomic.Pointer`, which its collector makes as short as SGCL does. The classic C++ answers, with the objects shared the way C++ shares them, by `shared_ptr`: the `std` container of `shared_ptr` under a `std::mutex`, and, for the queue and the stack, the same lock-free algorithm on the standard library's atomic operations on `shared_ptr`, which it implements with a lock; for the maps and the set, the `std` container under a `std::mutex` and under a `std::shared_mutex` with the lookups as readers. Every thread pushes an element and pops one, 200,000 times; nanoseconds per push or pop:

| container, threads | SGCL | `std::shared_ptr` | atomic `shared_ptr` | Go | Java ZGC |
|---|---|---|---|---|---|
| queue, 1 | 25.5 | 21.3 | 71.0 | 15.3 | 36.5 |
| queue, 4 | 100.0 | 61.8 | 198.8 | 75.8 | 116.9 |
| queue, 16 | 166.0 | 44.9 | 603.4 | 344.2 | 170.2 |
| queue, 32 | 179.5 | 48.4 | — | 513.4 | 185.9 |
| queue, 64 | 179.5 | 50.2 | — | 520.6 | 193.5 |
| stack, 1 | 13.4 | 21.4 | 51.6 | 13.8 | 48.6 |
| stack, 4 | 13.9 | 59.6 | 171.1 | 13.8 | 122.1 |
| stack, 16 | 16.6 | 42.1 | 137.9 | 38.5 | 311.0 |
| stack, 32 | 24.2 | 45.1 | 133.2 | 122.1 | 386.6 |
| stack, 64 | 27.3 | 45.6 | 138.5 | 122.6 | 392.8 |

The maps and the set: 200,000 keys inserted by the threads (disjoint, interleaved), then 200,000 lookups per thread of random keys present, then 200,000 mixed operations per thread over twice the key range, 80% lookups, 10% insertions, 10% erasures; nanoseconds per insertion / lookup / mixed operation. The `std` columns are `std::map`, `std::unordered_map` and `std::set` under the lock named:

| container, threads | SGCL | `std`, `std::mutex` | `std`, `std::shared_mutex` | Go | Java ZGC |
|---|---|---|---|---|---|
| map, 1 | 214 / 322 / 310 | 151 / 197 / 171 | 154 / 214 / 189 | 146 / 293 / 269 | 174 / 480 / 385 |
| map, 4 | 84 / 91 / 106 | 232 / 533 / 749 | 497 / 295 / 829 | 61 / 82 / 91 | 213 / 156 / 143 |
| map, 16 | 46 / 27 / 37 | 218 / 237 / 333 | 761 / 255 / 2382 | 74 / 24 / 35 | 308 / 71 / 79 |
| map, 32 | 42 / 26 / 33 | 244 / 265 / 418 | 848 / 254 / 2245 | 60 / 25 / 36 | 275 / 50 / 86 |
| map, 64 | 45 / 27 / 36 | 236 / 227 / 384 | 919 / 231 / 3625 | 80 / 23 / 39 | 453 / 39 / 78 |
| map, 1 | 88 / 67 / 72 | 39 / 39 / 40 | 46 / 52 / 50 | 151 / 80 / 84 | 121 / 99 / 108 |
| map, 4 | 30 / 24 / 24 | 125 / 144 / 204 | 254 / 157 / 437 | 75 / 21 / 29 | 177 / 38 / 60 |
| map, 16 | 20 / 4.9 / 15 | 100 / 62 / 144 | 324 / 94 / 1095 | 61 / 8.2 / 8.1 | 346 / 54 / 56 |
| map, 32 | 19 / 5.7 / 11 | 107 / 67 / 171 | 342 / 120 / 1174 | 55 / 5.8 / 8.8 | 442 / 39 / 68 |
| map, 64 | 17 / 5.0 / 9.8 | 117 / 71 / 185 | 377 / 118 / 1850 | 65 / 5.4 / 8.0 | 421 / 17 / 77 |
| set, 1 | 227 / 314 / 327 | 47 / 107 / 117 | 61 / 116 / 145 | 131 / 284 / 277 | 173 / 375 / 400 |
| set, 4 | 87 / 91 / 104 | 181 / 398 / 627 | 390 / 294 / 782 | 56 / 75 / 89 | 165 / 114 / 145 |
| set, 16 | 43 / 26 / 36 | 151 / 219 / 296 | 590 / 211 / 1824 | 45 / 23 / 34 | 262 / 60 / 134 |
| set, 32 | 41 / 25 / 32 | 167 / 255 / 328 | 614 / 225 / 1806 | 59 / 21 / 39 | 536 / 51 / 73 |
| set, 64 | 44 / 26 / 35 | 191 / 213 / 320 | 714 / 213 / 2705 | 59 / 22 / 37 | 402 / 34 / 66 |

The queue and the stack are a compare-exchange on a word every thread contends for, and what a compare-exchange loses to a lock under that contention is the retries: sixteen threads at one word fail far more exchanges than they win, and Java's deque, which has no backoff, spends 300 ns per operation on them where the mutex serializes the threads at 42 (Go's stack, with the same backoff and pause as SGCL's, is at 39). The stack answers with the backoff of Herlihy and Shavit, a wait that doubles after every lost exchange up to `config::BackoffMax` pauses, which turns the storm into near-serial exchanges: 17 ns per operation at sixteen threads and 14 at four, the fastest column of the table at every count, and 13 ns on one thread, where nothing is ever lost and the backoff never runs (23, 18 and 17 in the previous version of this table: a push used to wake the waiters of `pop()` whether there were any or not, and now looks at their count first, as the bounded queue does, which took the notify's fetch-add off every push). The queue answers with Java's hopping head and tail, which lag a node behind and are swung every second node, halving the exchanges on the two words: 166 ns at sixteen threads against 344 for the plain Michael–Scott queue in Go and 170 for Java's, though still nearly four times the mutex (a backoff gains the queue nothing, its retries being the walk to the first element rather than lost exchanges, and the map nothing either, whose threads lose an exchange only when they insert or erase neighbours under the same predecessor and whose search starts over from the top anyway; both measured). On one thread the container costs SGCL 26 and 13 ns (an `Item` and a node allocated per push, a hazard pointer per load), Go 15 and 14, Java 37 and 49, and the `std` container of `shared_ptr` under an uncontended mutex 21. What the table does not show at sixteen threads is why a lock-free structure is there at all when a mutex serializes as fast: no thread ever waits for a thread that was descheduled, killed or blocked while holding the word, which is the property the collector itself is built on, and the one a signal handler or a real-time thread needs. The rows at 32 and 64 threads, more threads than cores, show it in numbers: the lock-free stack stays at 12 and 13 ns where Go's, with the same algorithm, goes to 67 and Java's to 103 and 114, and `concurrent_stack` at 24 and 27 where Go's is at 122, since a thread preempted between its load and its exchange costs the others one lost exchange and nothing more, while a thread preempted with a mutex holds everyone behind it for its time slice (the mutex stack stays at 45 by luck of the scheduler's slices: a descheduled holder is rare when the critical section is a few nanoseconds, and every thread parks in the kernel anyway). The atomic `shared_ptr` column is the same algorithm with a lock inside every load, and its queue does not survive 32 threads at all: a consumer descheduled with a popped node keeps every node popped after it alive through the `next` links, and their release, recursive, runs off the end of the stack (the dash in the table).

The map is where the contended word disappears, since threads work on different nodes, and there the lock-free structure is what it is for, at 64 threads on 24 cores most of all (36 ns per mixed operation against 384 under the mutex and 3625 under the read-write lock, whose writers wait for readers that were descheduled holding it; Go's skip list 39): from four threads up SGCL's map is faster than `std::map` under either lock on every operation, and at sixteen threads by nine to sixty times (37 ns per mixed operation against 333 under the mutex and 2382 under the read-write lock, whose writers starve the readers); Java's `ConcurrentSkipListMap` is at 79. Go's hand-written skip list is the fastest at four threads and level with SGCL at sixteen: a link in Go is a plain load where SGCL's `atomic::load` takes a hazard pointer (a store and a load the processor may not reorder, a few nanoseconds per step of the search, and a search over 200,000 keys is some thirty steps; the byte of state the `tracked_ptr` built on the stack writes costs nothing measurable, being written once per object per cycle). That is the whole of the one-thread column, where SGCL is the slowest map of the table (322 ns per lookup against Go's 293, and 197 for the red-black tree under an uncontended mutex): a load-side cost, per step, that an asymmetric barrier (the fence on the collector's side instead of the mutator's) would take off the loads; without the hazard pointer the same search measures 285 ns. The insertion columns of the map and the set are the SGCL cells measured again after the whole run (best of three, the rest of the row from the run): `try_emplace` and the set's `insert` used to look the key up and then search again for the neighbours to link between, two searches of 200 ns each on this one-thread phase, where the keys arrive in order and every load hits the cache; they search once now, and build the node only when the key is absent (443 and 456 ns per insertion before, 214 and 227 in the run the rows are from; 151 and 154 to 84 and 87 at four threads, 67 to 46 and 43 at sixteen, the allocation between the search and the link costing nothing measurable in lost exchanges). What is left against Go's 146 is the walk itself: a search of this shape loads 31 links, and each is a hazard pointer and a `tracked_ptr` built, assigned and dropped, 8 ns a step where Go's is a load; the same walk on raw pointers measures 90 ns. `copy_on_write` over an array of 64 `long`s, three or fifteen readers each taking 2,000,000 snapshots and summing them while one writer replaces the value as fast as it can (a copy with one element changed): nanoseconds per read across the readers / per write, against `std::shared_ptr` with the atomic operations of `<memory>`, the array under a `std::shared_mutex` changed in place, Go's `atomic.Pointer` to an array swapped the same way, and Java's `CopyOnWriteArrayList` of 64 `Long`s:

| threads | SGCL | atomic `shared_ptr` | `std::shared_mutex` | Go | Java ZGC |
|---|---|---|---|---|---|
| 4 | 8.9 / 202 | 66.1 / 722 | 321 / 196 | 10.3 / 532 | 70.3 / 174 |
| 16 | 3.1 / 406 | 30.3 / 2145 | 89.1 / 3417 | 2.8 / 715 | 33.8 / 901 |
| 32 | 1.6 / 444 | 32.6 / 3880 | 75.0 / 4466 | 1.7 / 1321 | 14.7 / 698 |
| 64 | 1.0 / 828 | 32.8 / 10779 | 65.6 / 12015 | 1.4 / 1126 | 8.6 / 1055 |

A read is a load and a sum of 64 words: SGCL and Go, whose readers touch nothing shared, take 3 ns per read across fifteen readers, which is the sum alone spread over them; the atomic `shared_ptr` pays its lock and its count on every load (30 ns), the `shared_mutex` its readers contending for the one word of the lock (89, and the writer starved to 3.4 µs per change), Java's list the iterator and the unboxing (34). A write is the copy of 64 words and the exchange, and its cost is the readers': the writer's store has to take the line of the pointer from the readers reloading it, and the faster they read, the more often. SGCL's readers are the fastest of the table, so its writer pays more than Java's at four threads (202 ns against 174, Java's readers reloading eight times less often; with the readers slowed down sixty-four times, measured, the same write takes 89 ns); with fifteen readers every column pays, SGCL 400, Go 700, Java 900. The copy and the exchange themselves are a small part of it: the writer with a plain store in place of the exchange, or without the copy, measures within 20%. The `shared_mutex` writer changes in place and takes longer still, waiting for the readers to leave.

`channel<T>` between *n* / 2 producers of 200,000 items each and as many consumers, a rendezvous (capacity 0) and a buffer of 64; nanoseconds per item end to end. The producers and consumers are threads (the `threads` column) or tasks on the scheduler (`tasks`: `co_await ch.async_send`, `co_await ch.async_receive`), against `std::queue` under a mutex with two condition variables (the classic bounded queue), Go's channel between goroutines, and Java's `SynchronousQueue` and `ArrayBlockingQueue`:

| capacity, n | SGCL threads | SGCL tasks | `std::queue`, mutex | Go | Java ZGC |
|---|---|---|---|---|---|
| 0, 2 | 381 | 153 | 3391 | 148 | 237 |
| 0, 4 | 679 | 210 | 3024 | 182 | 275 |
| 0, 16 | 628 | 582 | 3279 | 323 | 1305 |
| 0, 32 | 443 | 626 | 9158 | 273 | 1918 |
| 0, 64 | 519 | 598 | 11328 | 235 | 1884 |
| 64, 2 | 106 | 34 | 131 | 50 | 176 |
| 64, 4 | 134 | 94 | 295 | 73 | 213 |
| 64, 16 | 432 | 295 | 1023 | 102 | 526 |
| 64, 32 | 460 | 413 | 788 | 78 | 503 |
| 64, 64 | 550 | 506 | 856 | 71 | 669 |

A rendezvous is a handoff, and its cost is waking the other side. Between threads SGCL parks a thread on its atomic and wakes it through the kernel, 380 to 680 ns per item against 3 to 3.4 µs for the mutex and the condition variables (two of them, and every hand-off through both), 150 to 320 for Go, whose goroutines park and wake in user space, and 240 to 1300 for Java. Between tasks the wake is a push on the scheduler's queue and the woken task runs next on the same worker, which is Go's mechanism: 153 ns per item at two tasks against Go's 148, 210 at four against 182 (362 and 526 in the previous version of this table: the queue the waiters are kept in notified the threads waiting in its blocking pop on every push, nobody ever waiting there, a fetch-add and a fence on a table the process shares and a wake through the kernel now and then; gated on a count of the waiting, the async benchmarks page); at sixteen 582 against 323, where sixteen tasks over eight workers wake each other through the kernel. The buffered channel is a lock-free ring (the bounded queue of Vyukov: a slot with a sequence number each, one compare-exchange per operation, no allocation per element) against Go's lock and buffer: 34 ns per item at two tasks against Go's 50 and the mutex queue's 131, 94 at four against 73 and 295, and at sixteen 300 to 430 against 102, where sixteen producers and consumers race on the ring's two words. The two-task cell was 70 in the previous version of this table: a send that had pushed looked for a waiting receiver to serve and a receive that had popped for a waiting sender to move in, each look two loads through hazard pointers at the head of a queue that is empty nearly always, 10 ns each, and a send and a receive on one thread with nobody waiting cost 60 ns against the bare ring's 9; a buffered channel counts the entries of each list beside it now and looks at the count, 30 ns for the pair (a rendezvous, which serves every element through the lists, keeps no count: the two exchanges per entry on one line cost sixteen threads on a rendezvous half as much again, measured). The SGCL cells of this table are from a run after that change, the other columns kept; a spin before the park of a waiting thread, the queues' remedy, was measured on the channel and gained nothing: the waits are not where its time goes. The threads column at a capacity of 64 is the ring plus a park in the kernel whenever a side catches up with the other, and how often that happens in a run of 200,000 items (20 to 30 ms) is the machine's: the two-thread cell was 68 in the previous version of this table and is 115 to 200 today, on either version's code built side by side, while the tasks cell, which needs no kernel, is 70 either time. A channel under a lock in place of the ring, Go's shape, was measured too, with the operations that find the lock taken queued for its holder to run (flat combining) and their tasks suspended meanwhile: slower everywhere, three times at sixteen, because a suspension and its resume cost a microsecond through the scheduler where Go's goroutine parks and wakes for a tenth of that, and every operation that loses the lock pays it; the ring stays.

The hash map is the structure where a lookup is a few steps, not thirty, and there the table turns: at sixteen threads SGCL's `concurrent_map` looks up in 6 to 7 ns and does a mixed operation in 15, level with Go's `sync.Map` (8.2 and 8.1, a map built for reads from a snapshot), eight and four times ahead of Java's `ConcurrentHashMap` (54 and 56) and nine ahead of `std::unordered_map` under a mutex (62 and 144); on one thread it is at 85 ns per lookup against the mutex map's 39, the hazard pointer of every one of its four or five loads, and level with Go and Java. Its lookup at 32 and 64 threads was 41 and 115 ns in the first run of those rows, against 5.7 and 5.0 now: the hash map's array doubles as the elements outnumber its buckets, and a new bucket got its dummy node, the point its lookups start from, only from the first insertion into it, a lookup walking from the nearest initialized ancestor's dummy instead; with the array doubled late in a run of insertions and then only looked up, half the buckets and more had no dummy, several levels of them, and the ancestor's segment, which the split order interleaves with every uninitialized descendant's, was a large part of the list (a find of 6 ns took 7000, measured: 200,000 keys inserted by 32 threads, then found by one). A lookup makes the missing dummy now, as an insertion does and as the split-ordered list of Shalev and Shavit has it, once per bucket for the array's life. Its insertion at sixteen threads is 20 ns where the previous version of this table had 57 (and 30 at four against 58, 88 on one thread against 118): the table used to decide its growth on every insertion, from a count every thread wrote, and several threads grew it at once, and it looks once per `slots / 128` net insertions now and one thread grows it; then `try_emplace` searched twice, a lookup and the insertion's own search, and searches once, building the node only when the key is absent, as the sorted map's does (the same one search took an erasure by key from 220 to 170 ns on one thread here and from 780 to 480 in the sorted map, the node unlinked with the predecessor of the search that found it). The SGCL cells of this row and of the map and set rows, of the intern, weak-map and cache tables and the broadcast table were measured again after those changes, in one run (best of three, the same script), the other columns kept. The set is the skip list again, the map's numbers within their spread. The `sgcl::` maps and sets cost nothing measurable beyond `sgcl::` at four threads and above, their root words read once per operation, and the check of the iterator's word on one thread (121 against 85 ns per lookup of the hash map); the `sgcl::` queue and stack pay the location check on the loaded head and the new node, 5 to 6 ns per operation. These runs are short by design, a few tenths of a second each, so the collector's start and the first faults of the heap are a visible part of the one-thread columns.

## The bounded queue, the priority queue, intern and the weak map

The structures added in September 2026, measured with `benchmarks/compare.sh` (`CASES="bqueue pqueue intern wmap"`, the best of three runs, the machine under its desktop load of four to six) against what Go and Java have for each: the bounded queue against Go's buffered channel and Java's `ArrayBlockingQueue`, the priority queue against `container/heap` under a mutex (Go's library has no concurrent one) and Java's `PriorityBlockingQueue`, `intern` against Go's `unique.Make` and Java's `String.intern`, the weak map against a `WeakHashMap` under `Collections.synchronizedMap` (Go has none). The single-producer queue and the cache, which have no counterpart in either standard library, follow in the last section.

The bounded queue, `threads / 2` producers of 200 k items each and as many consumers, ns per item (the channel table above is the same harness over the channel, which is this ring with the machinery of `select` around it):

| capacity, threads | `concurrent_bounded_queue` | Go `chan` | Java `ArrayBlockingQueue` |
|---|---|---|---|
| 64, 2 | 52 | 51 | 186 |
| 64, 4 | 68 | 75 | 212 |
| 64, 16 | 101 | 109 | 572 |
| 64, 32 | 143 | 79 | 578 |
| 64, 64 | 187 | 73 | 658 |
| 1024, 2 | 36 | 52 | 150 |
| 1024, 4 | 31 | 66 | 86 |
| 1024, 16 | 70 | 82 | 68 |
| 1024, 32 | 78 | 71 | 101 |
| 1024, 64 | 142 | 66 | 87 |

The sixteen-thread cells were 266 and 208 at first, two and a half times Go's, with the backoff of a lost exchange on the ring's positions capped at 32 pauses; a sweep of the cap (128, 256, 512, 1024, 2048, 4096: 150/114, 121/90, 138/89, 106/84, 145/88, 144/70 at the two capacities, two and four threads unchanged) put it at 1024, where the lost exchanges of eight producers at one word, and of eight consumers at the other, become near-serial ones, as the stack's backoff does at its head; measured alone, best of five, the cells were 80 and 60 then, and are 101 and 70 in the run above.

The priority queue, mixed: every thread pushes a pseudo-random priority and pops the least, 200 k times, over an empty queue (it stays as short as the number of threads) and over one holding 100 k elements, ns per operation; SGCL's is a binary heap under a spin-then-park lock (it was a lock-free skip list first: 56, 183 and 351 ns at 1, 4 and 16 threads on the empty queue, and the reason for the change):

| queue, threads | `concurrent_priority_queue` | `std::priority_queue`, mutex | Go `container/heap`, mutex | Java `PriorityBlockingQueue` |
|---|---|---|---|---|
| empty, 1 | 12.5 | 21 | 15 | 29 |
| empty, 4 | 14.2 | 74 | 109 | 28 |
| empty, 16 | 26 | 51 | 127 | 24 |
| empty, 32 | 60 | 59 | 139 | 25 |
| empty, 64 | 69 | 63 | 139 | 28 |
| 100 k, 1 | 70 | 84 | 98 | 90 |
| 100 k, 4 | 71 | 356 | 187 | 122 |
| 100 k, 16 | 70 | 178 | 265 | 96 |
| 100 k, 32 | 84 | 171 | 244 | 79 |
| 100 k, 64 | 87 | 164 | 222 | 71 |

`intern`, a pool of 1000 distinct strings of 13 characters, every thread interning 1 M drawn at random, ns per intern:

| threads | `intern<string>` | Go `unique.Make` | Java `String.intern` |
|---|---|---|---|
| 1 | 69 | 33 | 89 |
| 4 | 19.5 | 8.5 | 25 |
| 16 | 5.2 | 2.3 | 12 |
| 32 | 4.8 | 2.3 | 9.8 |
| 64 | 4.6 | 2.1 | 6.8 |

The weak map, 10 k live objects as the keys, every thread doing 1 M operations on random ones, half insertions (kept if the object has no entry) and half lookups, ns per operation:

| threads | `concurrent_weak_map` | Java `WeakHashMap`, synchronized |
|---|---|---|
| 1 | 55 | 35 |
| 4 | 15 | 55 |
| 16 | 3.8 | 51 |
| 32 | 3.5 | 53 |
| 64 | 3.3 | 49 |

What the numbers say. The ring is the structure of Go's channel without Go's lock, and with room to run (a capacity of 1024) it is ahead of Go at every count, 36, 31 and 70 ns per item against 52, 66 and 82, level with Java's `ArrayBlockingQueue` at sixteen (68, its lock handing the ring to one thread at a time) and ahead of it at two and four; at a capacity of 64 the producers and consumers meet at the ring's two words and the wait for a slot is the cost, and there it is level with Go at every count (52 against 51 at two threads, 68 against 75 at four, 101 against 109 at sixteen, where Go's lock queues the goroutines in user space and the ring's sixteen threads back off from each other's exchanges); the two-thread cell was 75 in the previous version of this table, the handoff through the kernel once a side's spin is over, which the run above met less often (the channel's threads column, the same ring with the machinery of `select`, shows the same swing the other way). The priority queue is the one structure that changed its design on these numbers: as a lock-free skip list it lost to every heap under a lock, two to fourteen times per operation, because a skip list's front is the cache lines every consumer writes while a heap under a lock is a few words in one thread's cache at a time; as a heap under a spin-then-park lock it is ahead of Java's `PriorityBlockingQueue` (the same design, under a `ReentrantLock`) in five cells of six and of Go's heap under a mutex everywhere, the lock spun through for the few dozen nanoseconds of another thread's operation where the system's mutex parks (74 against 14 ns at four threads) and Go's queues its goroutines (109). `intern` scales as a lock-free hash set does, thirteen times from one thread to sixteen, level with Java's string table and two to three times behind Go's `unique`, whose map is built for this one operation (a lookup by a hash kept with the value and a clone of the string only when new). The weak map is the map with the weak key under it: a lookup and an insertion at 3.8 ns per operation across sixteen threads against Java's 51, the synchronized map serializing every thread; on one thread Java's `WeakHashMap` is twice as fast, a plain hash map against the split-ordered list's hazard pointer per load.

## The single-producer queue and the cache

The two structures without a counterpart in Go's or Java's library, measured on 18 September 2026 with the same script (`CASES="spsc cache"`) against what C++ has for each; the immutable containers have [a page of their own](../containers/im/benchmarks.md).

The single-producer single-consumer queue, one thread pushing 200,000 items and one popping them, the channel's harness at two threads (an `Item` allocated per push, a `tracked_ptr` moved through the ring), ns per item; the bounded queue at two threads and Go's and Java's are in the table above:

| capacity | `spsc_queue` |
|---|---|
| 64 | 27 |
| 1024 | 21 |

The cache, 10,000 entries over 20,000 keys (so that half the lookups hit), every thread doing 1,000,000 operations, 90% lookups and 10% insertions, ns per operation; `concurrent_cache` (a sampled LRU: a hit writes nothing shared, an insertion at capacity evicts the oldest of eight) against the classic C++ cache, `std::unordered_map` and a `std::list` of the order under a `std::mutex` (an exact LRU: every hit splices the list):

| threads | `concurrent_cache` | `map` + `list`, mutex |
|---|---|---|
| 1 | 73 | 39 |
| 4 | 26 | 163 |
| 16 | 22 | 137 |
| 32 | 23 | 188 |
| 64 | 23 | 230 |

The broadcast (a class of the async module, measured here with the containers, `CASES="bcast"`): one thread sending 1,000,000 values into a ring of 1024, every subscriber receiving every one, ns per value sent; the subscribers threads with the blocking receive, or tasks on the scheduler with `co_await`; Go has no broadcast in its library, so its column is the idiom, a channel of 1024 per subscriber with a goroutine receiving on each, the sender sending every value to each in turn; Java has none either:

| subscribers | `broadcast`, threads | `broadcast`, tasks | Go, a channel each |
|---|---|---|---|
| 1 | 135 | 142 | 46 |
| 4 | 620 | 1024 | 199 |
| 16 | 1469 | 6543 | 1249 |
| 64 | 192960 | 25310 | 13500 |

The broadcast's wait is kept with the subscription (the position waited for, the task's frame or the thread's park word, a link in the list of subscriptions the senders walk): the sender that commits past a registration claims it and wakes that one subscriber, once, and a thread looks for the next commit for a few microseconds before it parks, as the queues do. It was a round before, a channel every waiting subscriber registered on anew for every value and the sender closed: 40 µs per value with sixteen thread subscribers and 209 with sixty-four in the first run of this table, 1.5 and 193 now, 74 to 135 with one (the walk of the subscriptions and the spin), 1021 to 620 with four; with task subscribers 8.5 and 31.5 µs at sixteen and sixty-four before, 6.5 and 25 now. A send costs the node's allocation and a load per subscription; a receive between threads costs the node's count, one line all the subscribers write per value, and the spin; between tasks it costs the wake, a push on the scheduler's queue and a worker woken for it, some 400 ns against 75 for Go's goroutine, which is where the task column lags Go's: at one subscriber the tasks are level with the threads, at four and at sixteen they are five times Go's. Sixty-four thread subscribers on twenty-four cores park in the kernel and are woken one by one, and a subscriber parked long enough is lapped by the ring and loses values (97% received in that cell): that row is the machine's, not the broadcast's; the tasks, which share the workers, take 25 µs there, Go's goroutines 13.5.

What the numbers say. The single-producer ring was 75 and 66 in the first run of this table, behind the bounded queue's 36 to 53 on the same harness, and the reason was found the same day: it was Lamport's ring then, each side caching the other's index, and a consumer at the producer's heels (the producer allocates, the consumer only sums) reloaded the producer's tail at nearly every pop, two lines crossing between the cores per element where the bounded queue's cell, its sequence and its element on one line, crosses once; rewritten on a sequence per cell, the bounded queue's cell without its compare-exchange, it is 27 and 21, ahead of the bounded queue (36 to 53) and of every other column of the two-thread rows, and 5.7 ns per `int` between two threads on its own page against 21 to 27 for the ring it replaced. The cache is the map with the stamps: on one thread the list under an uncontended mutex is nearly twice as fast (39 ns against 73, a hash lookup and a splice against the split-ordered list's hazard pointer per load, the stamp and the stripe), and from four threads up the lock is the cost, since every hit of an exact LRU is a write to the list under it, 137 to 163 ns, where the sampled cache, whose hit writes nothing shared, is at 26 and 22, six times ahead (29 at sixteen threads before the count of the entries, which every insertion writes, was moved off the line of the tick, which every lookup reads). The broadcast's send allocates a node per value and its receivers copy the value out and count it off the node, one line shared by all of them per value: 135 ns with one subscriber, 620 with four, the four contending for every node's count; a receiver that looked at the slot before the commit word, to spare itself the word's line on a hit, was measured and dropped (93 to 127 with one subscriber, 1520 to 1570 with four, on the round-based version): the readers polling the slot's line slowed the sender about to write it.
