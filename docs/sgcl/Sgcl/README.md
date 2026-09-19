# The Sgcl reference

One page per public class or function of the interface, each with every public member, its signature as declared in the header, and an example that compiles. The guide, the rules and the benchmarks are in the [main README](../../../README.md); this is the reference to come back to. Every page names its counterpart in the `sgcl` interface, [`docs/sgcl/`](../README.md), whose pages name theirs here: the two interfaces are one library, and the pages are written so that a class can be compared side by side.

## What the interface is

The library behind an object-oriented face: PascalCase types and methods, a class over each type of the `sgcl` interface with the one object inside, the same semantics, cost and rules. For a program written in the style of C# or Java rather than of the standard library: `list.Add(x)`, `dictionary.ContainsKey(k)`, `Ptr p = Make<Node>()`.

- **A class per type, by composition.** `List<T>` holds one buffer handle and nothing else; every method is an inline forward, so a `List` costs what the buffer costs, byte for byte and nanosecond for nanosecond. `Inner()` is the object inside, as its own type, for the code that wants that interface (the node handles of a dictionary, the bucket interface); an explicit constructor takes such an object over.
- **Values and pointers, as in C++.** A `List<T>` is a value: a copy is a copy, `==` compares the elements. Sharing is a pointer: `Ptr shared = Make<List<int>>();`, and `Ptr` is the pointer the collector follows, so nobody owns and nothing is freed by hand. This is not a clone of Java's reference semantics.
- **Nothing checked that the library does not check.** `list[i]` is unchecked, `First()` on an empty list is what `front()` is, there are no exceptions of the interface's own; a lookup that may fail hands back a pointer (`Find`, null when absent), an `Optional` (`Receive`, `TryGet`) or `NoIndex`/`NoPosition`. Where the library throws (`Value()` of an `Expected` without one, `As<T>()` of an `Any` holding another type), the face throws the same.
- **The rules are the rules of the type inside** ([The rules](../core/README.md#the-rules)): a `Ptr`, a `List`, a `Channel` live on a stack or in a managed object, never in unmanaged memory; a `RootPtr` lives anywhere.
- **`begin`/`end` are free functions**, not members, so that a range-for works (`for (auto& x : list)`) and the class keeps to its own names; the iterators are the buffer's or the nodes'. `std::ranges::sort(list)` and `std::sort(begin(l), end(l))` work too.
- **`Make<T>(args...)`** is the one way an object enters the managed heap: a `UniquePtr<T>`, which a `Ptr`, a `RootPtr` or an `Atomic` takes over from the expression itself.
- **Four modules**, one directory each in `sgcl/Sgcl/` and here, mirroring those of `sgcl`, each depending only on those before it and each with a README of its own that describes it and lists its classes: [Core](Core/README.md) (`sgcl/Sgcl/Core/Core.h`: the collector, `Ptr`, `UniquePtr`, `RootPtr`, `WeakPtr`, `Make`, `Variant`, `Any`, `Function`, `Expected`, `Range`), [Containers](Containers/README.md) (`sgcl/Sgcl/Containers/Containers.h`: `List`, `Dictionary`, `String`, `ExpiryQueue` and the rest), [Concurrent](Concurrent/README.md) (`sgcl/Sgcl/Concurrent/Concurrent.h`: the lock-free containers, `ConcurrentBoundedQueue`, `ConcurrentPriorityQueue`, `ConcurrentCache`, the concurrent weak containers, `Intern`, the persistent structures, `Atomic`, `CopyOnWrite`), [Async](Async/README.md) (`sgcl/Sgcl/Async/Async.h`: `Task`, `Executor`, `TaskLocal`, `Channel`, `Select`, `Broadcast`, time and the clock, `Signals`, `StopToken`, `Sync`, `TaskGroup`, `Timeout`, `Promise`, `SpawnBlocking`, the reactor, `Thread`). The sections below list the same pages by what they are.
- **No prefix.** The headers bring the namespace in (`using namespace Sgcl;`), so a program writes `List<int>`, `Make<Node>()`, `Spawn(f())` as the examples do; a name of the program's own that is also the interface's (`Scheduler`, `String`, `Any`) is spelled `Sgcl::` where the two meet.

## Pointers

| page | header | what it is |
|---|---|---|
| [Ptr](Core/Ptr.md) | `sgcl/Sgcl/Core/Ptr.h` | the pointer the collector follows: one word, a write barrier, no count; aliases, `Type()`, `Is<U>()`, `As<U>()`, `IfAlive()`, `ToShared()` |
| [UniquePtr](Core/UniquePtr.md) | `sgcl/Sgcl/Core/Ptr.h` | what `Make` returns: a `std::unique_ptr` to a managed object, deterministic until moved into a `Ptr` |
| [Make](Core/Make.md) | `sgcl/Sgcl/Core/Ptr.h` | creates an object on the managed heap |
| [RootPtr](Core/RootPtr.md) | `sgcl/Sgcl/Core/Ptr.h` | a root that lives anywhere (a global, a `std` container, a lambda on the heap): a cell of a managed block under it, the `Ptr` it holds its object by one step away; the pointer of an interpreter's handle table or a program's globals |
| [WeakPtr](Core/WeakPtr.md) | `sgcl/Sgcl/Core/Ptr.h` | a pointer that does not keep its object alive, cleared by the cycle that finds the object unreachable |
| [WeakDictionary, WeakMultiDictionary](Containers/WeakDictionary.md) | `sgcl/Sgcl/Containers/WeakDictionary.h` | values attached to objects the dictionary does not keep alive: keyed by the object, an entry dies with it |
| [WeakHashSet](Containers/WeakHashSet.md) | `sgcl/Sgcl/Containers/WeakDictionary.h` | a set of objects it does not keep alive |
| [Variant](Core/Variant.md) | `sgcl/Sgcl/Core/Variant.h` | `std::variant`'s interface with the tracked pointers in a word of their own, apart from the data of the other alternatives |
| [Any](Core/Any.md) | `sgcl/Sgcl/Core/Any.h` | `std::any`'s interface with a tracked pointer in a word of its own and an object with pointers in a managed node of its own |
| [Function, MoveOnlyFunction](Core/Function.md) | `sgcl/Sgcl/Core/Function.h` | `std::function` and `std::move_only_function` whose closure may capture tracked pointers: the closure in a managed node of its own |
| [Expected](Core/Expected.md) | `sgcl/Sgcl/Core/Expected.h` | `std::expected`'s interface (C++23) over a variant: the value and the error laid out apart |
| [Optional, None, Pair](Core/Types.md); [Range](Core/Range.md) | `sgcl/Sgcl/Core/Types.h`, `sgcl/Sgcl/Core/Range.h` | the `std` types under the interface's names: they hold a tracked pointer correctly as they are, one value per place; a `Range` is a pair of iterators for a range-for, or the integers of `Range(n)` |
| [String](Core/String.md) | `sgcl/Sgcl/Core/String.h` | an immutable string on the managed heap: one word, shared by copying, compared and hashed by its contents, no destructor; `ToString(number)` |
| [StringView](Core/StringView.md) | `sgcl/Sgcl/Core/String.h` | a view of a String that holds the string's object: two words, a substring with no copy and no lifetime to watch; what the pieces of `Split` are |
| [StringBuilder](Core/StringBuilder.md) | `sgcl/Sgcl/Core/StringBuilder.h` | the scratch buffer a String is built in: `Append`, then `ToString()` once; a `std::string` under the interface's names, no tracked pointer, lives anywhere |
| [Atomic, AtomicRef](Concurrent/Atomic.md) | `sgcl/Sgcl/Concurrent/Atomic.h` | lock-free atomic `Ptr`: `Load`, `Store`, compare-exchange, `Wait`/`Notify`, no ABA ([AtomicRef](Concurrent/AtomicRef.md) on its own page) |

## Containers

The interfaces of `std` with the names of a collection library, the nodes and buffers on the managed heap: a container lives where a `Ptr` may live, its elements are destroyed exactly when `std` destroys them, and the collector reclaims the memory.

| page | `std` counterpart |
|---|---|
| [List](Containers/List.md) | `std::vector` |
| [Array](Containers/Array.md) | `std::array`, and a buffer sized at creation |
| [Deque](Containers/Deque.md) | `std::deque` |
| [LinkedList](Containers/LinkedList.md) | `std::list` |
| [ForwardList](Containers/ForwardList.md) | `std::forward_list` |
| [Stack](Containers/Stack.md) | `std::stack` |
| [Queue, PriorityQueue](Containers/Queue.md) | `std::queue`, `std::priority_queue` |
| [SortedDictionary](Containers/SortedDictionary.md) | `std::map` |
| [SortedMultiDictionary](Containers/SortedMultiDictionary.md) | `std::multimap` |
| [SortedSet](Containers/SortedSet.md) | `std::set` |
| [SortedMultiSet](Containers/SortedMultiSet.md) | `std::multiset` |
| [Dictionary](Containers/Dictionary.md) | `std::unordered_map` |
| [MultiDictionary](Containers/MultiDictionary.md) | `std::unordered_multimap` |
| [HashSet](Containers/HashSet.md) | `std::unordered_set` |
| [HashMultiSet](Containers/HashMultiSet.md) | `std::unordered_multiset` |
| [OrderedDictionary](Containers/OrderedDictionary.md) | a hash map in insertion order (Java `LinkedHashMap`, .NET `OrderedDictionary`) |
| [OrderedSet](Containers/OrderedSet.md) | a hash set in insertion order (Java `LinkedHashSet`) |

The algorithms of a sequence (`Contains`, `IndexOf`, `Find`, `Sort`, `Reverse`, `Min`, `ForEach`...) are members of every sequence, from one mixin (`M`: a static interface, brought in by a template; an `I` will name a polymorphic one, when a module needs it):

| page | header | what it is |
|---|---|---|
| [MSequence](Containers/MSequence.md) | `sgcl/Sgcl/Containers/MSequence.h` | the mixin (`M`): the algorithms as members of `List`, `Array`, `Deque`, `LinkedList`, `ForwardList`; static, no virtual method, no converting to it |

## Lock-free containers

Structures shared by any number of threads without a lock, the textbook algorithms with no reclamation scheme in them, because the collector is one; the interfaces of `java.util.concurrent` and `System.Collections.Concurrent`.

| page | header | what it is |
|---|---|---|
| [ConcurrentQueue](Concurrent/ConcurrentQueue.md) | `sgcl/Sgcl/Concurrent/ConcurrentQueue.h` | the Michael–Scott queue: unbounded, FIFO, `Enqueue`, `TryDequeue`, a blocking `Dequeue` |
| [ConcurrentStack](Concurrent/ConcurrentStack.md) | `sgcl/Sgcl/Concurrent/ConcurrentQueue.h` | the Treiber stack: one word, `Push`, `TryPop`, a blocking `Pop` |
| [ConcurrentBoundedQueue](Concurrent/ConcurrentBoundedQueue.md) | `sgcl/Sgcl/Concurrent/ConcurrentBoundedQueue.h` | Vyukov's bounded MPMC queue: a ring of cells with sequence numbers, one compare-exchange per operation, no allocation per element, `TryEnqueue`, `TryDequeue`, a blocking `Enqueue` and `Dequeue` |
| [SpscQueue](Concurrent/SpscQueue.md) | `sgcl/Sgcl/Concurrent/SpscQueue.h` | Lamport's ring for one producer and one consumer, each caching the other's index: wait-free, no compare-exchange, the same interface |
| [ConcurrentPriorityQueue](Concurrent/ConcurrentPriorityQueue.md) | `sgcl/Sgcl/Concurrent/ConcurrentPriorityQueue.h` | a binary heap under a spin-then-park lock, what Java's `PriorityBlockingQueue` is (the skip-list one lost to it): the least element first, equal ones FIFO, `Enqueue`, `TryDequeue`, a blocking `Dequeue`, `TryPeek` |
| [ConcurrentSortedDictionary](Concurrent/ConcurrentSortedDictionary.md) | `sgcl/Sgcl/Concurrent/ConcurrentDictionary.h` | the lock-free skip list of Herlihy and Shavit: an ordered dictionary with `Find`, `TryGet`, `Add`, `GetOrAdd`, `Remove`, weakly consistent iteration |
| [ConcurrentSortedSet](Concurrent/ConcurrentSortedSet.md) | `sgcl/Sgcl/Concurrent/ConcurrentHashSet.h` | the same skip list with the value as the element |
| [ConcurrentDictionary](Concurrent/ConcurrentDictionary.md) | `sgcl/Sgcl/Concurrent/ConcurrentDictionary.h` | the split-ordered list of Shalev and Shavit: a lock-free hash dictionary that doubles its bucket array without moving a node |
| [ConcurrentHashSet](Concurrent/ConcurrentHashSet.md) | `sgcl/Sgcl/Concurrent/ConcurrentHashSet.h` | the same table with the value as the element |
| [ConcurrentCache](Concurrent/ConcurrentCache.md) | `sgcl/Sgcl/Concurrent/ConcurrentCache.h` | a cache over the hash dictionary bounded by a capacity and a time to live, the least recently used evicted by sampling: `TryGet`, `Set`, `GetOrAdd`, `Hits`, `Misses` |
| [ConcurrentWeakDictionary](Concurrent/ConcurrentWeakDictionary.md) | `sgcl/Sgcl/Concurrent/ConcurrentWeakDictionary.h` | the WeakDictionary shared by any number of threads: values attached to objects the dictionary does not keep alive, over the lock-free hash table, the dead entries swept by the inserting threads |
| [ConcurrentWeakHashSet](Concurrent/ConcurrentWeakHashSet.md) | `sgcl/Sgcl/Concurrent/ConcurrentWeakDictionary.h` | the same table with the objects alone: a set of objects it does not keep alive, shared by the threads |
| [CopyOnWrite](Concurrent/CopyOnWrite.md) | `sgcl/Sgcl/Concurrent/CopyOnWrite.h` | a value read by many threads and replaced whole: one load for an immutable snapshot, a copy and a compare-exchange for a change |
| [Intern](Concurrent/Intern.md) | `sgcl/Sgcl/Concurrent/Intern.h` | a pool where equal values share one managed object (Go's `unique`, Java's `String.intern`): `Make(value)` the canonical object, held weakly, compared by identity; `InternString` for Strings |
| [PersistentList](Concurrent/PersistentList.md) | `sgcl/Sgcl/Concurrent/PersistentList.h` | the persistent vector of Clojure and Scala: a 32-way trie with a tail, every `Add`, `RemoveLast` and `Set` a new version sharing all but a path |
| [PersistentDictionary](Concurrent/PersistentDictionary.md) | `sgcl/Sgcl/Concurrent/PersistentDictionary.h` | the persistent hash dictionary: Bagwell's hash array mapped trie, every `Set` and `Remove` a new version sharing all but a path; transparent lookup |
| [PersistentSet](Concurrent/PersistentSet.md) | `sgcl/Sgcl/Concurrent/PersistentDictionary.h` | the same trie with the value as the element |
| [Channel](Async/Channel.md) | `sgcl/Sgcl/Async/Channel.h` | the channel of Go: a buffered or rendezvous queue that threads and coroutines send to and receive from, waiting on either side, closed to end the stream |
| [Select, AsyncSelect, Otherwise](Async/Select.md) | `sgcl/Sgcl/Async/Channel.h` | the select of Go: a wait on several channels at once, a receive or a send per case with a body, `Otherwise` for a poll; for a thread or a task |
| [Broadcast](Async/Broadcast.md) | `sgcl/Sgcl/Async/Broadcast.h` | a channel every subscriber receives every value from (tokio's broadcast, Kotlin's SharedFlow): one ring, a cursor per Subscription, a send that never waits, a subscriber that falls behind lapped and told how many it lost |
| [Sleep, After, Tick, Timeout](Async/Time.md) | `sgcl/Sgcl/Async/Time.h` | time: a task suspended for a while, a channel signalled once after a while or every while, a select case served after a while; one timer thread under them |
| [Sleep, SleepUntil, After, At, Tick, Timeout](Async/Time.md) | `sgcl/Sgcl/Async/Time.h` | time: a task suspended for a while or until a point, a channel signalled once after a while or at a point or every while, a select case served after a while; one timer thread under them |
| [Clock, ManualClock](Async/Clock.md) | `sgcl/Sgcl/Async/Time.h` | `Clock::Now()`, the module's time in one place, and the clock of a test: installed, time moves only by `Advance(d)`, which fires every timer due with no real waiting |
| [Signals](Async/Signal.md) | `sgcl/Sgcl/Async/Signal.h` | `Signals({SIGINT, SIGTERM})`: the signals of the process as a channel, for a task, a thread or a Select; `ResetSignals`, `IgnoreSignals` |
| [StopSource, StopToken](Async/StopToken.md) | `sgcl/Sgcl/Async/StopToken.h` | cancellation: a source requests the stop, a token is a channel closed by it (a Select case, an awaitable), a deadline is a timer, a child source stops with its parent |
| [WhenAll, WhenAny](Async/When.md) | `sgcl/Sgcl/Async/When.h` | the composition of tasks: every result as a tuple or a List, or the index of the first to finish |
| [Mutex](Async/Mutex.md) | `sgcl/Sgcl/Async/Mutex.h` | one holder at a time, a channel holding one signal: blocking, awaitable and as a Select case; a task waiting holds no thread |
| [Semaphore](Async/Semaphore.md) | `sgcl/Sgcl/Async/Semaphore.h` | n permits, a channel holding n signals; the three forms of a wait |
| [Event](Async/Event.md) | `sgcl/Sgcl/Async/Event.h` | set once, waited for by any number: a channel closed by the set |
| [WaitGroup](Async/WaitGroup.md) | `sgcl/Sgcl/Async/WaitGroup.h` | counts work down to zero, a channel per round closed at zero (Go's WaitGroup) |
| [Once](Async/Once.md) | `sgcl/Sgcl/Async/Once.h` | the first caller runs it, the others wait for it, blocking or awaitable |
| [Promise](Async/Promise.md) | `sgcl/Sgcl/Async/Promise.h` | a one-shot completion set once by any thread or C callback, awaited by a task, blocked on by a thread, a case of a Select; the adapter between a platform's callbacks and `co_await` |
| [SpawnBlocking, BlockingTask, BlockingPool](Async/Blocking.md) | `sgcl/Sgcl/Async/Blocking.h` | a blocking call on a pool of threads apart from the workers, its result back through a Promise; the pool grows on demand to a cap and its idle threads exit |
| [TaskGroup](Async/TaskGroup.md) | `sgcl/Sgcl/Async/TaskGroup.h` | structured concurrency: a scope that owns the tasks it spawns, waited for as one (a thread, a task, a Select case), stopped as one by the first exception, which the wait rethrows; Go's `errgroup`, Kotlin's `coroutineScope` |
| [Timeout, WithDeadline, TimedOut](Async/Timeout.md) | `sgcl/Sgcl/Async/Timeout.h` | a timeout on a task: `Timeout(t, d)` the result or None, `WithDeadline(t, d)` the result or `TimedOut` thrown, a token as the deadline; the loser stopped through its source or left to finish |
| [Mutex](Async/Mutex.md) | `sgcl/Sgcl/Async/Mutex.h` | one holder at a time, a channel holding one signal: blocking, awaitable and as a Select case; a task waiting holds no thread |
| [Semaphore](Async/Semaphore.md) | `sgcl/Sgcl/Async/Semaphore.h` | n permits, a channel holding n signals; the three forms of a wait |
| [Event](Async/Event.md) | `sgcl/Sgcl/Async/Event.h` | set once, waited for by any number: a channel closed by the set |
| [WaitGroup](Async/WaitGroup.md) | `sgcl/Sgcl/Async/WaitGroup.h` | counts work down to zero, a channel per round closed at zero (Go's WaitGroup) |
| [Once](Async/Once.md) | `sgcl/Sgcl/Async/Once.h` | the first caller runs it, the others wait for it, blocking or awaitable |
| [SharedMutex](Async/SharedMutex.md) | `sgcl/Sgcl/Async/SharedMutex.h` | any number of readers or one writer, over a word, writer preference (Go's RWMutex); blocking and awaitable |
| [ConditionVariable](Async/ConditionVariable.md) | `sgcl/Sgcl/Async/ConditionVariable.h` | Go's Cond over the Mutex: a wait lets go of the mutex, waits for a notify and takes it back; blocking and awaitable |
| [Readable, Writable](Async/Reactor.md) | `sgcl/Sgcl/Async/Reactor.h` | the reactor: the readiness of a file descriptor as a channel, one thread on the kernel's queue (kqueue; epoll and IOCP to come); the foundation of io and net |

## Coroutines, observers, the collector

| page | header | what it is |
|---|---|---|
| [ManagedFrame, FramePtr, Task, Generator](Async/Coroutine.md) | `sgcl/Sgcl/Async/Coroutine.h` | coroutine frames on the managed heap, whose locals and parameters are roots; a task is spawned, joined, awaited, detached, and starts with the first wait for it; `ManagedFrame` and `FramePtr` for a coroutine type of your own |
| [AsyncGenerator](Async/AsyncGenerator.md) | `sgcl/Sgcl/Async/AsyncGenerator.h` | a generator that may wait: `co_yield`s values and `co_await`s between them, consumed from a task with `co_await g.Next()` |
| [Scheduler](Async/Scheduler.md) | `sgcl/Sgcl/Async/Scheduler.h` | the pool of workers that runs the tasks: `Spawn`, `Go`, `Yield`, `Scheduler::Stop`; a task that waits holds no thread |
| [Executor, Strand, On, OnWorkers](Async/Executor.md) | `sgcl/Sgcl/Async/Executor.h` | a task on a thread of the program's choosing (the main thread, a foreign loop through `Poll`), resumed there after every wait; a strand: tasks on the workers one at a time, in order; `co_await On(ex)`, `co_await OnWorkers()` |
| [TaskLocal](Async/TaskLocal.md) | `sgcl/Sgcl/Async/TaskLocal.h` | a value visible to a task and to the tasks it starts, read from any function under it: `co_await x.Set(v)`, `x.Get()`, `x.With(v, t)`; inherited, copy on write |
| [Thread, ThisThread](Async/Thread.md) | `sgcl/Sgcl/Async/Thread.h` | `std::thread` and `std::this_thread` under the interface's names: `Join`, `Detach`, `IsJoinable`; `ThisThread::SleepFor`, `Yield`; the threads kept in a `List<Thread>` |
| [ExpiryQueue](Containers/ExpiryQueue.md) | `sgcl/Sgcl/Containers/ExpiryQueue.h` | a callback for an object the collector found unreachable, with the object alive again for the call |
| [Collector](Core/Collector.md) | `sgcl/Sgcl/Core/Collector.h` | `Collect`, `Terminate`, statistics and phase times, live objects and bytes by type, the memory limit |
| [config](../core/config.md) | `sgcl/core/config.h` | the compile-time constants and the `-D` macros that set them |
| [diagnostics](../../garbage_collector/diagnostics.md) | | the tools and the cases: what is alive and why, what a rule broken looks like, what a cycle costs, a race with the collector |
| [how it works](../../garbage_collector/how-it-works.md) | | the engine: the heap, a slot's states, the barrier, the roots, a cycle phase by phase, epochs and parity, young and full cycles, the weak phase, the cells of the `RootPtr`s and when to use which pointer, allocation |

## Reading the pages

Every page has the same layout: the include and the declaration, its counterpart in the `sgcl` interface, what the class is and how it differs from `std`, the rules that apply to it (where an object of the class may live, what it may hold, thread safety, what happens in destructors), the members in the order of the header, each with its signature and a short example, one complete program at the end, and links to the related pages and README sections. The examples use C++20 class template argument deduction (`Ptr p = Make<T>();`) and no prefix, as the headers bring the namespace in, and every `Collect()` in them is optional, there to show the result at once.
