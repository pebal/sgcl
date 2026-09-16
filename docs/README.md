# SGCL reference

One page per public class or function of the library, each with every public member, its signature as declared in the header, and an example that compiles. The guide, the rules and the benchmarks are in the [main README](../README.md); this is the reference to come back to.

## Pointers

| page | header | what it is |
|---|---|---|
| [tracked_ptr](tracked_ptr.md) | `sgcl/tracked_ptr.h` | the pointer the collector follows: one word, a write barrier, no count; aliases, `type()`, `is<U>()`, `as<U>()`, `if_alive()` |
| [unique_ptr](unique_ptr.md) | `sgcl/unique_ptr.h` | what `make_tracked` returns: a `std::unique_ptr` to a managed object, deterministic until moved into a `tracked_ptr` |
| [make_tracked](make_tracked.md) | `sgcl/make_tracked.h` | creates an object on the managed heap |
| [root_ptr](root_ptr.md) | `sgcl/root_ptr.h` | a root that lives anywhere (a global, a `std` container, a lambda on the heap): a cell of a managed block under it, the `tracked_ptr` it holds its object by one step away; the pointer of an interpreter's handle table or a program's globals |
| [weak_ptr](weak_ptr.md) | `sgcl/weak_ptr.h` | a pointer that does not keep its object alive, cleared by the cycle that finds the object unreachable |
| [string](string.md) | `sgcl/string.h` | an immutable string on the managed heap: one word, shared by copying, compared and hashed by its contents, no destructor |
| [weak_map, weak_multimap](weak_map.md) | `sgcl/weak_map.h` | values attached to objects the map does not keep alive: keyed by the object, an entry dies with it |
| [weak_set](weak_set.md) | `sgcl/weak_set.h` | a set of objects it does not keep alive |
| [variant](variant.md) | `sgcl/variant.h` | `std::variant`'s interface with the tracked pointers in a word of their own, apart from the data of the other alternatives |
| [any](any.md) | `sgcl/any.h` | `std::any`'s interface with a tracked pointer in a word of its own and an object with pointers in a managed node of its own |
| [function](function.md) | `sgcl/function.h` | `std::function` and `std::move_only_function` whose closure may capture tracked pointers: the closure in a managed node of its own |
| [expected](expected.md) | `sgcl/expected.h` | `std::expected`'s interface (C++23) over a variant: the value and the error laid out apart |
| `optional`, `pair`, `tuple` | `sgcl/aliases.h` | the `std` types under the library's names: they hold a tracked pointer correctly as they are, one value per place |
| [atomic, atomic_ref](atomic.md) | `sgcl/atomic.h`, `sgcl/atomic_ref.h` | lock-free atomic `tracked_ptr`: `load`, `store`, compare-exchange, `wait`/`notify`, no ABA ([atomic_ref](atomic_ref.md) on its own page) |

## Containers

The interfaces of `std`, the nodes and buffers on the managed heap: a container lives where a `tracked_ptr` may live, its elements are destroyed exactly when `std` destroys them, and the collector reclaims the memory.

| page | `std` counterpart |
|---|---|
| [vector](vector.md) | `std::vector` |
| [array](array.md) | `std::array`, plus a dynamic `array<T>` |
| [deque](deque.md) | `std::deque` |
| [list](list.md) | `std::list` |
| [forward_list](forward_list.md) | `std::forward_list` |
| [stack](stack.md) | `std::stack` |
| [queue, priority_queue](queue.md) | `std::queue`, `std::priority_queue` |
| [map](map.md) | `std::map` |
| [multimap](multimap.md) | `std::multimap` |
| [set](set.md) | `std::set` |
| [multiset](multiset.md) | `std::multiset` |
| [unordered_map](unordered_map.md) | `std::unordered_map` |
| [unordered_multimap](unordered_multimap.md) | `std::unordered_multimap` |
| [unordered_set](unordered_set.md) | `std::unordered_set` |
| [unordered_multiset](unordered_multiset.md) | `std::unordered_multiset` |

## Lock-free containers

Structures shared by any number of threads without a lock, the textbook algorithms with no reclamation scheme in them, because the collector is one; the interfaces of `java.util.concurrent` under the names of `std`.

| page | header | what it is |
|---|---|---|
| [concurrent_queue](concurrent_queue.md) | `sgcl/concurrent_queue.h` | the Michael–Scott queue: unbounded, FIFO, `push`, `try_pop`, a blocking `pop` |
| [concurrent_stack](concurrent_stack.md) | `sgcl/concurrent_stack.h` | the Treiber stack: one word, `push`, `try_pop`, a blocking `pop` |
| [concurrent_map](concurrent_map.md) | `sgcl/concurrent_map.h` | the lock-free skip list of Herlihy and Shavit: an ordered map with `find`, `insert`, `try_emplace`, `erase`, weakly consistent iteration |
| [concurrent_set](concurrent_set.md) | `sgcl/concurrent_set.h` | the same skip list with the key as the element |
| [concurrent_unordered_map](concurrent_unordered_map.md) | `sgcl/concurrent_unordered_map.h` | the split-ordered list of Shalev and Shavit: a lock-free hash map that doubles its bucket array without moving a node |
| [concurrent_unordered_set](concurrent_unordered_set.md) | `sgcl/concurrent_unordered_set.h` | the same table with the key as the element |
| [copy_on_write](copy_on_write.md) | `sgcl/copy_on_write.h` | a value read by many threads and replaced whole: one load for an immutable snapshot, a copy and a compare-exchange for a change |
| [channel](channel.md) | `sgcl/channel.h` | the channel of Go: a buffered or rendezvous queue that threads and coroutines send to and receive from, waiting on either side, closed to end the stream |

## Coroutines, observers, the collector

| page | header | what it is |
|---|---|---|
| [coroutine](coroutine.md) | `sgcl/coroutine.h` | `managed_frame`, `frame_ptr`, `task`, `generator`: coroutine frames on the managed heap, whose locals and parameters are roots |
| [expiry_queue](expiry_queue.md) | `sgcl/expiry_queue.h` | a callback for an object the collector found unreachable, with the object alive again for the call |
| [collector](collector.md) | `sgcl/collector.h` | `force_collect`, `terminate`, statistics and phase times, live objects and bytes by type, the memory limit |
| [config](config.md) | `sgcl/config.h` | the compile-time constants and the `-D` macros that set them |
| [diagnostics](diagnostics.md) | | the tools and the cases: what is alive and why, what a rule broken looks like, what a cycle costs, a race with the collector |
| [how it works](how-it-works.md) | | the engine: the heap, a slot's states, the barrier, the roots, a cycle phase by phase, epochs and parity, young and full cycles, the weak phase, the cells of the `root_ptr`s and when to use which pointer, allocation |

## Reading the pages

Every page has the same layout: the include and the declaration, what the class is and how it differs from `std`, the rules that apply to it (where an object of the class may live, what it may hold, thread safety, what happens in destructors), the members in the order of the header, each with its signature and a short example, one complete program at the end, and links to the related pages and README sections. The examples use C++20 class template argument deduction (`sgcl::tracked_ptr p = sgcl::make_tracked<T>();`) and every `force_collect()` in them is optional, there to show the result at once.
