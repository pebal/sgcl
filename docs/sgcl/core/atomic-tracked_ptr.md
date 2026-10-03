[sgcl](../README.md) › [core](README.md) › [atomic](atomic.md)

# sgcl::atomic\<tracked_ptr\<T\>\>

```cpp
#include "sgcl/core/atomic.h"   // or "sgcl/core.h"

namespace sgcl {
    template<class T>
    class atomic<tracked_ptr<T>>;
}
```

`sgcl::atomic<tracked_ptr<T>>` is `std::atomic` for a [tracked_ptr](tracked_ptr.md): a word that several threads
read and write without a lock, with `load`, `store`, `exchange`, `compare_exchange_weak`, `compare_exchange_strong`,
`wait` and `notify` taking a `std::memory_order`. It is the answer to rule 6 ([The rules](README.md#the-rules)): a
`tracked_ptr` written by one thread and read by another needs `atomic`, [atomic_ref](atomic_ref.md) or the
program's own synchronization. A global shared pointer is a [root_ptr](root_ptr.md) with an `atomic_ref` over it:
the `root_ptr` holds its object by a `tracked_ptr` in a managed block, and `atomic_ref` binds that word
([atomic_ref\<tracked_ptr\<T\>\>](atomic_ref-tracked_ptr.md)).

The difference from `std::atomic<std::shared_ptr<T>>` is that it is lock-free, one word, and safe against reuse: a
`load()` publishes a hazard pointer for the length of the load, so that the collector cannot reclaim the object
between the read of the word and the construction of the `tracked_ptr` that holds it, and once held the object
cannot be reclaimed at all. A compare-exchange therefore has no ABA problem: a node is never freed and reused while
any thread holds a `tracked_ptr` to it, so the address it compares against is the node it means. A lock-free stack
or queue needs no hazard pointers or epochs of its own (`benchmarks/concurrent/lockfree_stack.cpp`, and
[Benchmarks: Lock-free stack](../concurrent/benchmarks.md#lock-free-stack)).

## Rules

- An `atomic<tracked_ptr<T>>` holds a `tracked_ptr`, so it lives where one may: on a stack or inside a managed
  object, never in `new`/`malloc` memory, a `std` container or a global ([The rules](README.md#the-rules), 1 and
  6). A global shared pointer is a `root_ptr<T>` under an `atomic_ref`, or a `unique_ptr` to a managed object
  holding the atomic:
  `static sgcl::unique_ptr current = sgcl::make_tracked<sgcl::atomic<sgcl::tracked_ptr<Config>>>();`.
- It is neither copyable nor movable, like `std::atomic`.
- Every operation is lock-free (`is_always_lock_free`) and may be called from any thread. A `load()` costs the read
  of the word twice around a hazard store; a `store()` or a compare-exchange the atomic operation and the write
  barrier.
- In a destructor an `atomic` member is a `tracked_ptr` member without `if_alive()`: its target may be dying in the
  same sweep, so a destructor does not read it ([The rules](README.md#the-rules), 5).

## Template parameters

| Parameter | Description |
|---|---|
| `T` | The type of the object the pointer addresses, as for `tracked_ptr<T>`. |

## Member types

| Type | Definition |
|---|---|
| `value_type` | `tracked_ptr<T>` |

## Member objects

| Constant | Value | Description |
|---|---|---|
| `is_always_lock_free` | `true` on every platform the library supports | `std::atomic<void*>::is_always_lock_free`: the word is a `std::atomic` of a pointer; `static constexpr bool` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](atomic-tracked_ptr/atomic-tracked_ptr.md) | constructs the atomic: null, or from a pointer |
| `(destructor)` | destroys the atomic; the object it pointed to is left to the collector |
| [operator=](atomic-tracked_ptr/operator_assign.md) | stores a pointer |
| [is_lock_free](atomic-tracked_ptr/is_lock_free.md) | checks whether the operations are lock-free |
| [store](atomic-tracked_ptr/store.md) | replaces the pointer |
| [load, operator tracked_ptr\<T\>](atomic-tracked_ptr/load.md) | reads the pointer, held |
| [exchange](atomic-tracked_ptr/exchange.md) | replaces the pointer and returns the old one, held |
| [compare_exchange_weak, compare_exchange_strong](atomic-tracked_ptr/compare_exchange.md) | replaces the pointer when it is the one expected |
| [wait](atomic-tracked_ptr/wait.md) | blocks while the pointer is the one given |
| [notify_one](atomic-tracked_ptr/notify_one.md) | wakes a thread blocked in `wait` |
| [notify_all](atomic-tracked_ptr/notify_all.md) | wakes every thread blocked in `wait` |

## Deduction guides

```cpp
template<class T>
atomic(unique_ptr<T>&&) -> atomic<tracked_ptr<T>>;
template<class T>
atomic(tracked_ptr<T>) -> atomic<tracked_ptr<T>>;
```

`atomic current = make_tracked<Config>()` and `atomic shared(node)` for a `tracked_ptr<Node> node` are
`atomic<tracked_ptr<…>>`.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

// A configuration replaced at run time under readers on other threads: the
// readers load it, the writer stores a new one, and the old one is
// collected when the last reader drops it. No lock, no count.
struct Config {
    explicit Config(int version) : version(version) {}
    int version;
};

int main() {
    atomic current = make_tracked<Config>(0);  // on main's stack, where a tracked_ptr may live
    atomic<int> backwards = 0;

    vector<thread> readers;
    for (int t : range(4)) {
        // by reference: main's frame outlives the threads it joins
        readers.emplace_back([&current, &backwards] {
            int last = -1;
            for (int i : range(100000)) {
                tracked_ptr config = current.load();  // held: cannot be reclaimed under this thread
                backwards += config->version < last;  // versions only go up
                last = config->version;
            }
        });
    }

    for (int version : range(1, 101)) {
        current.store(make_tracked<Config>(version));  // the old Config lives on for its readers
    }
    for (auto& r : readers) {
        r.join();
    }
    println("final version {}, {} seen going back", current.load()->version, backwards.load());
}
```

Output:

```text
final version 100, 0 seen going back
```

## See also

- [atomic_ref\<tracked_ptr\<T\>\>](atomic_ref-tracked_ptr.md): the same operations on a `tracked_ptr` that is not
  declared atomic
- [atomic\<H\>](atomic-handle.md): the atomic of a handle
- [tracked_ptr](tracked_ptr.md), [unique_ptr](unique_ptr.md), [make_tracked](make_tracked.md), [root_ptr](root_ptr.md)
- [README: Pointers](README.md#pointers), [README: The rules](README.md#the-rules),
  [Threads](../async/README.md#threads), [Benchmarks: Lock-free stack](../concurrent/benchmarks.md#lock-free-stack)
- `benchmarks/concurrent/lockfree_stack.cpp`: a lock-free stack on `atomic<tracked_ptr<T>>`
- [sgcl::atomic\<T\>](atomic.md)
