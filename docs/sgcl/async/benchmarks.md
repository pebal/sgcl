[sgcl](../README.md) › [async](README.md)

# Benchmarks: async

The setup, the machine, the environments and how the timers are read are described with [the benchmarks of the engine](../../garbage_collector/benchmarks.md); the numbers here were taken the same way, with `benchmarks/compare.sh` (`CASES=async`, the best of three runs of every case, the machine under its desktop load), from `benchmarks/async/async.cpp`, `benchmarks/go/async` and `benchmarks/java/Async.java`. The channel between threads and between tasks, against Go's channel and Java's queues, is on [the benchmarks of the concurrent module](../concurrent/benchmarks.md); this page is what the rest of the module costs on the scheduler: a hop, a wait, a race.

## The scheduler

Every case is a loop of one operation in a task on the scheduler's workers (an executor's thread where it says), in a goroutine on Go's runtime, in a virtual thread on Java's scheduler (JDK 26, ZGC); nanoseconds per operation, a dash where the language has no counterpart:

| Case | SGCL | Go | Java | Operation |
|---|---|---|---|---|
| yield | 25 | 100 | 1339 | `co_await async::yield()` on a worker: the frame pushed on the worker's ring and popped; `runtime.Gosched()`; `Thread.yield()` in a virtual thread |
| executor yield | 40 | — | — | `co_await async::yield()` on an [executor](executor.md)'s thread: the executor's queue, a list through the frames, nothing allocated per push |
| strand round trip | 80 | — | — | `co_await on_workers()` then `co_await on(strand)`: two hops through queues |
| await | 108 | 320 | 3292 | `co_await t` of a task that returns at once: its frame allocated, the task started, its end resuming the awaiter; a goroutine started and its answer received; a virtual thread started and joined |
| spawn | 120 | 322 | 3308 | `co_await async::spawn(t)` of the same task, one at a time: the await, with the scheduler's queue on the way in; the same in Go and Java |
| when_all | 219 | 300 | 2561 | `async::when_all(t, t)` of two such tasks, per task; two goroutines and a `WaitGroup`; two virtual threads joined |
| timeout | 495 | 689 | 4003 | `async::with_timeout(t, 1h)` of a task that returns at once, per race (measured 2026-09-26 after the change to `expected`; 533 before) ([with_timeout](with_timeout.md)); a `select` on the goroutine's channel and `time.After(1h)`; `CompletableFuture.orTimeout(1h)` completed by a virtual thread |
| select | 313 | 162 | — | `select` of a channel case with an element there and a `async::timeout(1h)` case: the case served at once, the timer armed and cancelled by the case's end; Go's `select` with `time.After` |
| condition variable | 160 to 610 | 131 | 1228 | a turn handed between two tasks through `condition_variable` and `mutex`, per hand-off; `sync.Cond`; `Condition` under a `ReentrantLock` |
| ping-pong | 170 | 135 | 228 | two tasks over two rendezvous channels, per hop; two goroutines over unbuffered channels; two virtual threads over `SynchronousQueue`s |
| generator | 9.1 | 0.3 | — | an `async::generator`'s value taken with `co_await g.next()`; a range-over-func iterator, which Go compiles into a loop |
| mutex | 12.3 | 7.7 | 14.4 | `co_await m.scoped_lock()` and the release, uncontended; `sync.Mutex`; `ReentrantLock` |

What the numbers say. The floor is the scheduler's hop: a yield on a worker is 25 ns, a frame through the worker's own ring, against Go's 100 for `Gosched` and Java's 1339 for a virtual thread's yield through its pool; an executor's yield is 40, through the executor's queue, a list linked through the frames themselves (it was 89 when the queue was a `concurrent::queue` with a managed node per push: the node was 4 ns of it, the rest the walk under hazard pointers that a queue with one consumer does not need). A task awaited costs 108 ns, its managed frame, its start and the hop of its end, where a goroutine started and answered is 320 and a virtual thread started and joined 3300; spawned and awaited 120 against 322 and 3308; `when_all` of two 219 per task against 300 and 2561. A race against a deadline is 495 ns against Go's 689 and Java's 4003 (533 before the race gave an `expected`): one object, a continuation that is a call into it at the task's end, a timer that calls into it too, and a compare-exchange between the two (1867 before, when the task ran under a runner task finishing into a slot, and the race was a task of its own selecting between the slot's channel and the timer's). The generator is the cheapest thing on the page, 9 ns per value, because it never goes near a queue: `next()` resumes the generator where the consumer runs and a `co_yield` resumes the consumer where the generator is; Go's 0.3 is not a switch at all, the iterator's body compiled into the caller's loop. The rendezvous between two tasks is 170 ns per hop, near Go's 135 and ahead of Java's 228, and a strand's round trip 80: both were 330 to 346 in an earlier version of this table, and the difference was one line in `concurrent::queue`, the queue the channel's lists of waiters are and the executors' queues were then: its push notified the threads waiting in `pop` whether there were any or not, which in libc++ is a fetch-add and a fence on a contention table the whole process shares, and a wake through the kernel whenever the table's entry happened to have a sleeping worker's word in it; gated on a count of the threads in `pop`, the notify is gone from a path that never blocks there. The strand's round trip went on from 138 to 80 when the executors' queue became a list through the frames, where a push allocates nothing and the one consumer takes without a hazard pointer. Where SGCL is still behind Go is the wait that needs more than a hand-off: the condition variable, 160 to 610 ns per hand-off against 131 (two modes, run by run: the pair split over two workers, or both on one, where the notifier takes the lock back before the notified runs), and the select with a channel's case served at once, 313 against 162. A wait on the condition variable allocates a channel of its own and registers on it, the mutex is let go of and taken back through its channel, and the notify goes through the queue of waiters: five allocations and as many registrations per hand-off, (the spurious wakes of the old notify used to split the pair over two workers every time, 311 per hand-off in the previous version of this table, at the price of a worker woken for nothing on every hop); a worker that runs out of work clears a page of its dead stack before looking for more, since the collector's scan sees the words the last task left there ([scheduler](scheduler.md)), some 20 ns on a hop that leaves a worker idle; Go's `sync.Cond` keeps a `sudog` per goroutine and allocates nothing. Java's virtual threads sit between: a `SynchronousQueue` hand-off is 228 ns, its condition 1228, and anything that starts a virtual thread is thousands. The mutexes are level, a compare-exchange each.

## Parallel loops

[parallel_for](parallel_for.md) against a plain loop and SNS-HDR's `parallel_for_`, the loop it is modelled on
(`benchmarks/async/parallel_for.cpp`, built as `bench_parallel_for`; SNS-HDR's side with
`-DSGCL_SNS_HDR_CORE=<SNS-HDR>/Core`). The kernel is a separable stack blur of a float image of 4096 × 4096 pixels
with a radius of 8, as SNS-HDR's blur filter runs it: the rows spread over the lanes, then the columns in groups of
eight (a loop with a step), each lane with a ring of scratch of its own indexed by its lane. Every variant computes
every pixel with the same code and prints the same checksum (`bench_parallel_for <variant> check` holds each against
the plain loop). Milliseconds per blur, the median of 400 blurs in a process (20 for the plain loop), the best of
three processes after one discarded; the CPU is the process's over the blurs; 24 lanes, the scheduler's default
workers and SNS-HDR's `Parallel::maxThreads()` threads, one per core (3 October 2026):

| Variant | Time per blur, ms | Against the plain loop | CPU per blur, ms |
|---|---|---|---|
| a plain loop on one thread | 85.9 | 1× | 86 |
| `async::parallel_for`, the default grain | 5.27 | 16.3× | 111 |
| `async::parallel_for`, a grain of 1 | 5.53 | 15.5× | 120 |
| `async::parallel_for` called inside a task | 5.33 | 16.1× | 112 |
| SNS-HDR's `parallel_for_` | 5.42 | 15.8× | 120 |

The four parallel variants are within 5 per cent of each other. The loop runs inside the call, the caller taking
chunks as lane 0, so a call from a task costs what a call from a thread does; the default grain, about eight chunks
a lane, claims a row or a group of columns less often than a grain of 1, which claims every index as SNS-HDR's
`parallel_for_` does, and spends 5 per cent less time and 7 per cent less CPU.

## Measured with the classes

Figures taken once, when a class was written or changed, on the same machine (Apple M-series, release builds),
and kept here rather than on the class pages.

- [channel](channel.md): a send and a receive on one thread are 30 ns with the count of each list of waiters kept
  beside it, against 60 without the count ([the concurrent benchmarks](../concurrent/benchmarks.md)).
- [broadcast](broadcast.md), with `benchmarks/compare.sh` (`bcast`, one sender thread of a million values, every
  subscriber receiving them all): 127 ns per value with one thread subscriber, 692 with four, 1.6 µs with sixteen,
  and 186 µs with sixty-four, more threads than the machine's cores, where the ones without a core park in the
  kernel and are woken one by one; with task subscribers 0.14, 0.87, 4.0 and 2.6 µs, the sender's walk handing the
  tasks it wakes to the scheduler's queues together and one worker woken for them. Go's idiom, a channel per
  subscriber and a goroutine on each: 46 ns, 199, 1249 and 13.5 µs (the table with the three columns is on
  [the concurrent benchmarks page](../concurrent/benchmarks.md#the-single-producer-queue-and-the-cache)). Before,
  each value was a *round*, a channel made by the first reader waiting for a position and closed by the sender that
  committed it, every reader registering on it anew for every value: sixteen subscribers cost 40 µs per value
  between threads and 8.5 between tasks.
- [shared_mutex](shared_mutex.md): a `lock_shared` and `unlock_shared` pair on one task 13 ns, on four tasks at
  once 51 ns (the word bouncing between the workers); the channel-built [mutex](mutex.md)'s `lock` and `unlock`
  pair 87 ns on one task, 393 ns on four contending (a receive and a send through the ring, and a waiter each time
  at four).
- [task_group](task_group.md), groups of 1000 children that return at once, 2000 rounds: `go` and `wait` together
  cost about 1150 ns per child from a thread, where every child goes through the global queue, and about 740 ns per
  child for a group made inside a task, where the children go on the worker's own ring; a `when_all` over a vector
  of 1000 tasks spawned from a thread costs about 1000 ns per task, so the group's own share, the runner's frame and
  the count, is about 150 ns.
- [timeout](timeout.md): a select with a long timeout in a loop keeps a bounded heap; before, a timer stayed per
  iteration until its deadline, 0.75 KB each, 300 MB for 400 k iterations.
- [manual_clock](manual_clock.md): with the clock's flag in place, `clock::now()` at 17 to 18 ns per call as
  `steady_clock::now()` was, a hundred thousand sleeping tasks fired through the heap in 135 to 145 ms as before, a
  timer armed in 455 to 465 ns as before (`tests/async/clock.cpp` and a probe of the three, before and after).
- [with_timeout](with_timeout.md): about 500 ns per race of a task that returns at once (495 in the table above),
  against 1870 for the runner task, the race task and the select it was before.
- [executor](executor.md) (`bench_async`): a round trip `co_await on(ex)` from a worker to an executor and
  `co_await on_workers()` back, about 640 ns; a round trip `co_await on_workers()` and `co_await on(s)` between the
  pool and a strand about 85 ns; a `yield` on an executor about 45 ns, against 26 ns for a `yield` on a worker.
- [spawn_blocking](spawn_blocking.md) (Apple M2 Ultra, macOS 26, Apple clang 21, `-O2`): the round trip of
  `co_await async::spawn_blocking([] {})`, a job that does nothing, is 6 to 7 µs, two hand-offs between threads
  through the kernel (the pool's thread woken on its condition variable and the task's worker woken by the set),
  against 140 ns for a promise made, set and awaited ready on one thread.

## How an A/B is measured

How an A/B of the scheduler or the channels is measured. Some cases are as fast as the page their structures landed on: the ping-pong of tasks over a rendezvous channel runs at 206 ns with the workers' rings on one page and at about 280 on the next, the same code, and one managed object more or less allocated at the start moves the rings by a page. So a comparison of two versions is taken in four placements: `SGCL_BENCH_PAD_PAGES=k` (0 to 3, `benchmarks/placement.h`, in `bench_async`, `bench_concurrent`, `bench_net` and the server of `http_load`) takes k managed pages before `main`, so that the rings and everything made after them land k pages further; each side runs five processes in each placement, the sides alternating, and the medians are compared, overall and per placement. For the task channels (`chan task` with capacity 0 and 64) the levels are compared, not the median: a median that moved because one side hit more slow placements than the other is not a cost of the code.

The time of such a series is the build of the sides that changed plus the measurement: a side that did not change runs its binaries from the last series (a directory per variant), and a change of the rule under test is a new side measured after the ones already built, never a replacement of one not yet measured.
