# The sgcl reference

One page per public class or function of the library, each with every public member, its signature as declared in the header, and an example that compiles. The guide, the rules and the benchmarks are in the [main README](../../README.md); this is the reference to come back to.

## Modules

The library is five modules, one directory each in `sgcl/` and here, each depending only on those before it and each with a README of its own that is its guide (what the classes are, the rules, what to reach for) before it lists them:

| module | header | what it holds |
|---|---|---|
| [core](core/README.md) | `sgcl/core/core.h` | the collector, `tracked_ptr`, `unique_ptr`, `root_ptr`, `weak_ptr`, `make_tracked`, `variant`, `any`, `function`, `expected`, `range`, the configuration |
| [containers](containers/README.md) | `sgcl/containers/containers.h` | the sequences, maps and sets, `string`, `weak_map`, `weak_set`, `expiry_queue`; the [immutable containers](containers/im/README.md) in `sgcl::im` (`im::vector`, `im::list`, `im::map`, `im::set`: every operation a new version sharing all but the path it changed; the state of a program as a value) |
| [concurrent](concurrent/README.md) | `sgcl/concurrent/concurrent.h` | the lock-free containers, `concurrent_bounded_queue`, `concurrent_priority_queue`, `concurrent_cache`, the concurrent weak containers, `intern`, `atomic`, `copy_on_write` |
| [async](async/README.md) | `sgcl/async/async.h` | coroutines, the scheduler, executors and strands, task-local values, `channel`, `select`, `broadcast`, timers and the clock, the signals of the process, `stop_token`, `when_all`/`when_any`, `task_group`, `timeout`, `sync`, `promise`, `spawn_blocking`, the reactor |
| [io](io/README.md) | `sgcl/io/io.h` | streams (`reader`, `writer`, `stream` and the mixins over one primitive), `buffered_reader` and `buffered_writer`, `file` over any descriptor with the pool and the reactor behind its async forms, the file system (`stat`, `mkdir_all`, `read_dir`, `walk_dir`), `path`, the process (`args`, `getenv`, the standard streams); errors as `result<T>`; the first module in a namespace of its own, `sgcl::io` |

The sections below list the same pages by what they are.

## Pointers

| page | header | what it is |
|---|---|---|
| [tracked_ptr](core/tracked_ptr.md) | `sgcl/core/tracked_ptr.h` | the pointer the collector follows: one word, a write barrier, no count; aliases, `type()`, `is<U>()`, `as<U>()`, `if_alive()`; `shade()` and the unshaded store for the copies of immutable nodes |
| [unique_ptr](core/unique_ptr.md) | `sgcl/core/unique_ptr.h` | what `make_tracked` returns: a `std::unique_ptr` to a managed object, deterministic until moved into a `tracked_ptr` |
| [make_tracked](core/make_tracked.md) | `sgcl/core/make_tracked.h` | creates an object on the managed heap |
| [root_ptr](core/root_ptr.md) | `sgcl/core/root_ptr.h` | a root that lives anywhere (a global, a `std` container, a lambda on the heap): a cell of a managed block under it, the `tracked_ptr` it holds its object by one step away; the pointer of an interpreter's handle table or a program's globals |
| [weak_ptr](core/weak_ptr.md) | `sgcl/core/weak_ptr.h` | a pointer that does not keep its object alive, cleared by the cycle that finds the object unreachable |
| [weak_map, weak_multimap](containers/weak_map.md) | `sgcl/containers/weak_map.h` | values attached to objects the map does not keep alive: keyed by the object, an entry dies with it |
| [weak_set](containers/weak_set.md) | `sgcl/containers/weak_set.h` | a set of objects it does not keep alive |
| [variant](core/variant.md) | `sgcl/core/variant.h` | `std::variant`'s interface with the tracked pointers in a word of their own, apart from the data of the other alternatives |
| [any](core/any.md) | `sgcl/core/any.h` | `std::any`'s interface with a tracked pointer in a word of its own and an object with pointers in a managed node of its own |
| [function](core/function.md) | `sgcl/core/function.h` | `std::function` and `std::move_only_function` whose closure may capture tracked pointers: the closure in a managed node of its own |
| [range](core/range.md) | `sgcl/core/range.h` | a pair of iterators as a range (what `equal_range` hands back, made iterable) and the integers of `range(n)`, `range(first, last)`; for a range-for and `std::ranges` |
| [string](core/string.md) | `sgcl/core/string.h` | an immutable string on the managed heap: one word, shared by copying, compared and hashed by its contents, no destructor |
| [slice](core/slice.md) | `sgcl/core/slice.h` | the elements of a contiguous range and the managed object they lie in, held: Go's slice; a `std::span` when the memory is unmanaged (no owner); `slice<const char>` is text, what `as_slice` and the pieces of `split` are, `slice<std::byte>` the buffers of io |
| [expected](core/expected.md) | `sgcl/core/expected.h` | `std::expected`'s interface (C++23) over a variant: the value and the error laid out apart |
| [optional, pair, tuple, error_code](core/aliases.md) | `sgcl/core/aliases.h` | the `std` types under the library's names: they hold a tracked pointer correctly as they are, one value per place |
| [atomic, atomic_ref](concurrent/atomic.md) | `sgcl/concurrent/atomic.h`, `sgcl/concurrent/atomic_ref.h` | lock-free atomic `tracked_ptr`: `load`, `store`, compare-exchange, `wait`/`notify`, no ABA ([atomic_ref](concurrent/atomic_ref.md) on its own page) |

## Containers

The interfaces of `std`, the nodes and buffers on the managed heap: a container lives where a `tracked_ptr` may live, its elements are destroyed exactly when `std` destroys them, and the collector reclaims the memory.

| page | `std` counterpart |
|---|---|
| [vector](containers/vector.md) | `std::vector` |
| [array](containers/array.md) | `std::array`, with the braces of an aggregate and the mixins of a range |
| [dynamic_array](containers/dynamic_array.md) | a count fixed at creation in a managed buffer that never moves: Java's `new T[n]` |
| [deque](containers/deque.md) | `std::deque` |
| [list](containers/list.md) | `std::list` |
| [forward_list](containers/forward_list.md) | `std::forward_list` |
| [stack](containers/stack.md) | `std::stack` |
| [queue, priority_queue](containers/queue.md) | `std::queue`, `std::priority_queue` |
| [sorted_map](containers/sorted_map.md) | `std::map` |
| [sorted_multimap](containers/sorted_multimap.md) | `std::multimap` |
| [sorted_set](containers/sorted_set.md) | `std::set` |
| [sorted_multiset](containers/sorted_multiset.md) | `std::multiset` |
| [map](containers/map.md) | `std::unordered_map` |
| [multimap](containers/multimap.md) | `std::unordered_multimap` |
| [set](containers/set.md) | `std::unordered_set` |
| [multiset](containers/multiset.md) | `std::unordered_multiset` |
| [ordered_map](containers/ordered_map.md) | a hash map in insertion order (Java `LinkedHashMap`) |
| [ordered_set](containers/ordered_set.md) | a hash set in insertion order (Java `LinkedHashSet`) |

The questions, the order and the writes of a range (`contains`, `index_of`, `find_if`, `sort`, `reverse`, `min`, `for_each`...) are members of every container that iterates, from the mixins (`m_`: a static interface, brought in by a template, and a declaration a concept (`c_`) can ask for; an `i_` will name a polymorphic one, when a module needs it):

| page | header | what it is |
|---|---|---|
| [mixin/](core/mixin/README.md) | `sgcl/core/mixin/mixin.h` | the mixins and the concepts: [m_enumerable](core/mixin/m_enumerable.md), [m_equatable](core/mixin/m_equatable.md), [m_comparable](core/mixin/m_comparable.md), [m_ordered](core/mixin/m_ordered.md), [m_sequence](core/mixin/m_sequence.md), [m_lookup](core/mixin/m_lookup.md), [m_text](core/mixin/m_text.md); [concepts](core/mixin/concepts.md): `c_enumerable`, `c_ordered`, `c_sequence`, `c_lookup`, `c_comparable`, ...; who carries what |

## Immutable containers

| page | header | what it is |
|---|---|---|
| [im::vector](containers/im/vector.md) | `sgcl/containers/im/vector.h` | Clojure's bit-partitioned trie with a tail: every `push_back`, `pop_back` and `sorted_set` a new version sharing all but a path |
| [im::list](containers/im/list.md) | `sgcl/containers/im/list.h` | the list of Lisp and ML: `push_front` one cell in front of the shared chain, `pop_front` the rest of it |
| [im::map](containers/im/map.md) | `sgcl/containers/im/map.h` | Bagwell's hash array mapped trie: every `insert` and `erase` a new version sharing all but a path; transparent lookup |
| [im::set](containers/im/set.md) | `sgcl/containers/im/set.h` | the same trie with the key as the element |

## Lock-free containers

Structures shared by any number of threads without a lock, the textbook algorithms with no reclamation scheme in them, because the collector is one; the interfaces of `java.util.concurrent` under the names of `std`.

| page | header | what it is |
|---|---|---|
| [concurrent_queue](concurrent/concurrent_queue.md) | `sgcl/concurrent/concurrent_queue.h` | the Michael–Scott queue: unbounded, FIFO, `push`, `try_pop`, a blocking `pop` |
| [concurrent_stack](concurrent/concurrent_stack.md) | `sgcl/concurrent/concurrent_stack.h` | the Treiber stack: one word, `push`, `try_pop`, a blocking `pop` |
| [concurrent_bounded_queue](concurrent/concurrent_bounded_queue.md) | `sgcl/concurrent/concurrent_bounded_queue.h` | Vyukov's bounded MPMC queue: a ring of cells with sequence numbers, one compare-exchange per operation, no allocation per element, `try_push`, `try_pop`, a blocking `push` and `pop` |
| [spsc_queue](concurrent/spsc_queue.md) | `sgcl/concurrent/spsc_queue.h` | a ring with a sequence per cell for one producer and one consumer, the cell the one line the two share: wait-free, no compare-exchange, the same interface |
| [concurrent_priority_queue](concurrent/concurrent_priority_queue.md) | `sgcl/concurrent/concurrent_priority_queue.h` | a binary heap under a spin-then-park lock, what Java's `PriorityBlockingQueue` is (the skip-list one lost to it): the least element first, equal ones FIFO, `push`, `try_pop`, a blocking `pop`, `try_top` |
| [concurrent_sorted_map](concurrent/concurrent_sorted_map.md) | `sgcl/concurrent/concurrent_sorted_map.h` | the lock-free skip list of Herlihy and Shavit: an sorted map with `find`, `insert`, `try_emplace`, `erase`, weakly consistent iteration |
| [concurrent_sorted_set](concurrent/concurrent_sorted_set.md) | `sgcl/concurrent/concurrent_sorted_set.h` | the same skip list with the key as the element |
| [concurrent_map](concurrent/concurrent_map.md) | `sgcl/concurrent/concurrent_map.h` | the split-ordered list of Shalev and Shavit: a lock-free hash map that doubles its bucket array without moving a node |
| [concurrent_set](concurrent/concurrent_set.md) | `sgcl/concurrent/concurrent_set.h` | the same table with the key as the element |
| [concurrent_cache](concurrent/concurrent_cache.md) | `sgcl/concurrent/concurrent_cache.h` | a cache over the hash map bounded by a capacity and a time to live, the least recently used evicted by sampling: `get`, `put`, `get_or_compute`, `hits`, `misses` |
| [concurrent_weak_map](concurrent/concurrent_weak_map.md) | `sgcl/concurrent/concurrent_weak_map.h` | the weak_map shared by any number of threads: values attached to objects the map does not keep alive, over the lock-free hash table, the dead entries swept by the inserting threads |
| [concurrent_weak_set](concurrent/concurrent_weak_set.md) | `sgcl/concurrent/concurrent_weak_set.h` | the same table with the objects alone: a set of objects it does not keep alive, shared by the threads |
| [copy_on_write](concurrent/copy_on_write.md) | `sgcl/concurrent/copy_on_write.h` | a value read by many threads and replaced whole: one load for an immutable snapshot, a copy and a compare-exchange for a change |
| [intern](concurrent/intern.md) | `sgcl/concurrent/intern.h` | a pool where equal values share one managed object (Go's `unique`, Java's `String.intern`): `make(value)` the canonical object, held weakly, compared by identity; `intern_string` for strings |
| [channel](async/channel.md) | `sgcl/async/channel.h` | the channel of Go: a buffered or rendezvous queue that threads and coroutines send to and receive from, waiting on either side, closed to end the stream |
| [select](async/select.md) | `sgcl/async/select.h` | the select of Go: a wait on several channels at once, a receive or a send per case with a body, `otherwise` for a poll; for a thread or a coroutine |
| [broadcast](async/broadcast.md) | `sgcl/async/broadcast.h` | a channel every subscriber receives every value from (tokio's broadcast, Kotlin's SharedFlow): one ring, a cursor per subscription, a send that never waits, a subscriber that falls behind lapped and told how many it lost |
| [timer](async/timer.md) | `sgcl/async/timer.h` | time: `sleep`, `sleep_until`, `after`, `at`, `tick`, `timeout`: a task suspended for a while or until a point, a channel signalled once after a while or at a point or every while, a select case served after a while; one timer thread under them |
| [clock](async/clock.md) | `sgcl/async/timer.h` | `clock::now()`, the module's time in one place, and `manual_clock`, the clock of a test: installed, time moves only by `advance(d)`, which fires every timer due with no real waiting |
| [signal](async/signal.md) | `sgcl/async/signal.h` | `signals({SIGINT, SIGTERM})`: the signals of the process as a channel, for a task, a thread or a select; `reset_signals`, `ignore_signals` |
| [stop_token](async/stop_token.md) | `sgcl/async/stop_token.h` | cancellation: `stop_source` requests the stop, `stop_token` is a channel closed by it (a select case, an awaitable), a deadline is a timer, a child source stops with its parent |
| [when](async/when.md) | `sgcl/async/when.h` | the composition of tasks: `when_all` (every result as a tuple or a vector), `when_any` (the index of the first to finish) |
| [mutex](async/mutex.md) | `sgcl/async/mutex.h` | one holder at a time, a channel holding one signal: blocking, awaitable and as a select case; a task waiting holds no thread |
| [semaphore](async/semaphore.md) | `sgcl/async/semaphore.h` | n permits, a channel holding n signals; the three forms of a wait |
| [event](async/event.md) | `sgcl/async/event.h` | set once, waited for by any number: a channel closed by the set |
| [wait_group](async/wait_group.md) | `sgcl/async/wait_group.h` | counts work down to zero, a channel per round closed at zero (Go's WaitGroup) |
| [once](async/once.md) | `sgcl/async/once.h` | the first caller runs it, the others wait for it, blocking or awaitable |
| [shared_mutex](async/shared_mutex.md) | `sgcl/async/shared_mutex.h` | any number of readers or one writer, over a word, writer preference (Go's RWMutex); blocking and awaitable |
| [condition_variable](async/condition_variable.md) | `sgcl/async/condition_variable.h` | Go's Cond over the mutex: a wait lets go of the mutex, waits for a notify and takes it back; blocking and awaitable |
| [promise](async/promise.md) | `sgcl/async/promise.h` | `promise<T>`: a one-shot completion set once by any thread or C callback, awaited by a task, blocked on by a thread, a case of a select; the adapter between a platform's callbacks and `co_await` |
| [blocking](async/blocking.md) | `sgcl/async/blocking.h` | `spawn_blocking`: a blocking call on a pool of threads apart from the workers, its result back through a promise; the pool grows on demand to a cap and its idle threads exit; `blocking_pool` for its statistics and stop |
| [task_group](async/task_group.md) | `sgcl/async/task_group.h` | structured concurrency: a scope that owns the tasks it spawns, waited for as one (a thread, a task, a select case), stopped as one by the first exception, which the wait rethrows; Go's `errgroup`, Kotlin's `coroutineScope` |
| [timeout](async/timeout.md) | `sgcl/async/timeout.h` | a timeout on a task: `timeout(t, d)` the result or nullopt, `with_deadline(t, d)` the result or `timed_out` thrown, a token as the deadline; the loser stopped through its source or left to finish |
| [reactor](async/reactor.md) | `sgcl/async/reactor.h` | `readable`, `writable`: the readiness of a file descriptor as a channel, one thread on the kernel's queue (kqueue; epoll and IOCP to come); the foundation of io and net |

## Files and streams

| page | header | what it is |
|---|---|---|
| [error, result](io/error.md) | `sgcl/io/error.h` | `errc`, `error` (code, operation, path; `is_not_found()`…), `result<T>`: every operation of io returns one, nothing throws |
| [stream](io/stream.md) | `sgcl/io/stream.h` | `reader`, `writer`, `seeker`, `closer`, `stream`: one pure virtual primitive each and a mixin with the rest (`read_full`, `read_all`, `copy_to`, `write_text`…), blocking and `co_await` forms; `copy`, `limit_reader`, `tee_reader`, `multi_reader`, `multi_writer`, `discard`, `buffer` |
| [buffered](io/buffered.md) | `sgcl/io/buffered.h` | `buffered_reader`: lines and prefixes as views into a managed block, `lines()`, a bound for untrusted streams; `buffered_writer`: `flush` |
| [file](io/file.md) | `sgcl/io/file.h` | `file` over any descriptor: `open`, `create`, `from_fd`, `pipe`, `read_at`/`write_at`, `stat`, `sync`; async through the blocking pool or the reactor; `read_file`, `write_file`, `append_file`, `temp_file`, `temp_dir` |
| [fs](io/fs.md) | `sgcl/io/fs.h` | `stat`, `lstat`, `file_info`, `permissions`, `mkdir_all`, `remove_all`, `rename`, `copy_file`, `symlink`, `chmod`, `read_dir`, `walk_dir` |
| [path](io/path.md) | `sgcl/io/path.h` | `clean`, `join`, `base`, `dir`, `ext`, `stem`, `split`, `abs`, `rel`, `match`, `glob`: paths as strings, a view in and a string out |
| [os](io/os.md) | `sgcl/io/os.h` | `args`, `getenv`, `environ`, `expand_env`, `working_dir`, `home_dir`, `cache_dir`, `executable`, `hostname`, `stdin`/`stdout`/`stderr` as files, `exit` |

## Coroutines, observers, the collector

| page | header | what it is |
|---|---|---|
| [coroutine](async/coroutine.md) | `sgcl/async/coroutine.h` | `managed_frame`, `frame_ptr`, `task`, `generator`, `async_generator`: coroutine frames on the managed heap, whose locals and parameters are roots; a task is spawned, joined, awaited, detached; a task starts with the first wait for it |
| [scheduler](async/scheduler.md) | `sgcl/async/scheduler.h` | the pool of workers that runs the tasks: `spawn`, `yield`, `scheduler::stop`; a task that waits holds no thread |
| [executor](async/executor.md) | `sgcl/async/executor.h` | `executor`: a task on a thread of the program's choosing (the main thread, a foreign loop through `poll`), resumed there after every wait; `strand`: tasks on the workers one at a time, in order; `co_await on(ex)`, `co_await on_workers()` |
| [task_local](async/task_local.md) | `sgcl/async/task_local.h` | a value visible to a task and to the tasks it starts, read from any function under it: `co_await x.set(v)`, `x.get()`, `x.with(v, t)`; inherited, copy on write |
| [thread](async/thread.md) | `sgcl/core/aliases.h` | `std::thread` and `std::this_thread` under the library's names; the threads kept in a `sgcl::vector<sgcl::thread>` |
| [expiry_queue](containers/expiry_queue.md) | `sgcl/containers/expiry_queue.h` | a callback for an object the collector found unreachable, with the object alive again for the call |
| [collector](core/collector.md) | `sgcl/core/collector.h` | `force_collect`, `terminate`, statistics and phase times, live objects and bytes by type, the memory limit |
| [config](core/config.md) | `sgcl/core/config.h` | the compile-time constants and the `-D` macros that set them |
| [diagnostics](../garbage_collector/diagnostics.md) | | the tools and the cases: what is alive and why, what a rule broken looks like, what a cycle costs, a race with the collector |
| [how it works](../garbage_collector/how-it-works.md) | | the engine: the heap, a slot's states, the barrier, the roots, a cycle phase by phase, epochs and parity, young and full cycles, the weak phase, the cells of the `root_ptr`s and when to use which pointer, allocation |

## Reading the pages

Every page has the same layout: the include and the declaration, what the class is and how it differs from `std`, the rules that apply to it (where an object of the class may live, what it may hold, thread safety, what happens in destructors), the members in the order of the header, each with its signature and a short example, one complete program at the end, and links to the related pages and README sections. The examples use C++20 class template argument deduction (`sgcl::tracked_ptr p = sgcl::make_tracked<T>();`) and every `force_collect()` in them is optional, there to show the result at once.
