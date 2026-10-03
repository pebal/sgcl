[sgcl](../../README.md) › [core](../README.md) › [atomic_ref](../atomic_ref.md)

# sgcl::atomic_ref\<tracked_ptr\<T\>\>

```cpp
#include "sgcl/core/atomic_ref.h"   // or "sgcl/core.h"

namespace sgcl {
    template<class T>
    class atomic_ref<tracked_ptr<T>>;
}
```

`sgcl::atomic_ref<tracked_ptr<T>>` is `std::atomic_ref` for a [tracked_ptr](../tracked_ptr/README.md): the operations of
[atomic\<tracked_ptr\<T\>\>](../atomic-tracked_ptr/README.md) (`load`, `store`, `exchange`, `compare_exchange_weak`,
`compare_exchange_strong`, `wait`, `notify`, with a `std::memory_order`) applied to a plain `tracked_ptr` that lives
somewhere already: a member of a node, an element of an `sgcl::vector<tracked_ptr<T>>`, a local. The word of a
`tracked_ptr` is a `std::atomic` of a pointer in any case, so nothing changes in the pointer's layout; the
`atomic_ref` is a reference to it and the operations are the atomic ones, with the hazard pointer of
`atomic::load` and the same freedom from ABA.

A [root_ptr](../root_ptr/README.md) converts to the `tracked_ptr` it holds its object by, the word of its cell in a managed
block, so `atomic_ref a(root)` (deduced, as from a `tracked_ptr`) is the atomic of a root that lives anywhere: a
global holding an object that other threads share and that is replaced at run time is a `root_ptr` under an
`atomic_ref`.

## Rules

- The `tracked_ptr` referred to lives where one may (a stack, a managed object), and outlives every `atomic_ref` to
  it: it must not be moved or destroyed while a view of it exists, as with any `atomic_ref`. The `atomic_ref` itself
  is a plain reference and may live anywhere, like `std::atomic_ref`.
- As with `std::atomic_ref`, while any `atomic_ref` to a `tracked_ptr` exists, the pointer is accessed only through
  `atomic_ref`s: a plain copy or assignment of the pointer at the same time is a data race in the program's terms
  (the word itself is atomic, so it is never torn, and the collector is correct under any interleaving:
  [The rules](../README.md#the-rules), 6).
- Copyable (the copy refers to the same pointer), not assignable.
- Every operation is lock-free (`is_always_lock_free`) and may be called from any thread. Every `tracked_ptr` has
  the alignment the operations need, since its word is that atomic.
- In a destructor the referred `tracked_ptr` is a member of a dying object: its target may be dying in the same
  sweep, so a destructor does not `load()` it ([The rules](../README.md#the-rules), 5).

## Template parameters

| Parameter | Description |
|---|---|
| `T` | The type of the object the pointer addresses, as for `tracked_ptr<T>`. |

## Member types

| Type | Definition |
|---|---|
| `value_type` | `tracked_ptr<T>` |

## Member objects

| Object | Value | Description |
|---|---|---|
| `ref` | the constructor's argument | the `tracked_ptr` referred to, `value_type&`, public: for a `root_ptr` the word it holds its object by |
| `is_always_lock_free` | `true` on every platform the library supports | `std::atomic<void*>::is_always_lock_free`; `static constexpr bool` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](atomic_ref-tracked_ptr.md) | constructs a view of a `tracked_ptr` |
| [operator=](operator_assign.md) | stores a pointer |
| [is_lock_free](is_lock_free.md) | checks whether the operations are lock-free |
| [store](store.md) | replaces the pointer |
| [load, operator tracked_ptr\<T\>](load.md) | reads the pointer, held |
| [exchange](exchange.md) | replaces the pointer and returns the old one, held |
| [compare_exchange_weak, compare_exchange_strong](compare_exchange.md) | replaces the pointer when it is the one expected |
| [wait](wait.md) | blocks while the pointer is the one given |
| [notify_one](notify_one.md) | wakes a thread blocked in `wait` |
| [notify_all](notify_all.md) | wakes every thread blocked in `wait` |

## Deduction guides

```cpp
template<class T>
atomic_ref(T&) -> atomic_ref<T>;
template <class T>
atomic_ref(tracked_ptr<T>) -> atomic_ref<tracked_ptr<T>>;
template <class T>
atomic_ref(root_ptr<T>) -> atomic_ref<tracked_ptr<T>>;
```

`atomic_ref(p)` for a `tracked_ptr<T> p` and `atomic_ref(r)` for a `root_ptr<T> r` are
`atomic_ref<tracked_ptr<T>>`, the second over the word `r` holds its object by.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

// A table of slots that several threads fill: the slots are plain
// tracked_ptrs in a managed buffer, and each claim is a compare-exchange
// through an atomic_ref. The first thread to claim a slot wins; the losers
// see its item and move on. No slot is ever a torn pointer, and an item
// that lost the race is garbage the collector reclaims.
struct Item {
    explicit Item(int owner) : owner(owner) {}
    int owner;
};

int main() {
    vector<tracked_ptr<Item>> slots(64);  // 64 null slots in a managed buffer
    atomic<int> won = 0, lost = 0;

    vector<thread> workers;
    for (int t : range(4)) {
        workers.emplace_back([&slots, &won, &lost, t] {  // the vector stays in main's frame
            for (size_t i : range(slots.size())) {
                tracked_ptr<Item> expected;  // null: the slot is free
                tracked_ptr mine = make_tracked<Item>(t);
                atomic_ref slot(slots[i]);
                if (slot.compare_exchange_strong(expected, mine)) {
                    ++won;
                } else {
                    lost += expected != nullptr;  // somebody else's item, now held by expected
                }
            }
        });
    }
    for (auto& w : workers) {
        w.join();
    }
    println("{} won, {} lost", won.load(), lost.load());
}
```

Output:

```text
64 won, 192 lost
```

## See also

- [atomic\<tracked_ptr\<T\>\>](../atomic-tracked_ptr/README.md): the same operations on a `tracked_ptr` declared atomic
- [atomic_ref\<H\>](../atomic_ref-handle/README.md): the atomic view of a handle
- [tracked_ptr](../tracked_ptr/README.md), [root_ptr](../root_ptr/README.md), [unique_ptr](../unique_ptr/README.md), [vector](../vector/README.md)
- [README: Pointers](../README.md#pointers), [README: The rules](../README.md#the-rules),
  [Threads](../../async/README.md#threads)
- `benchmarks/concurrent/lockfree_stack.cpp`: a lock-free stack on `atomic<tracked_ptr<T>>`
- [sgcl::atomic_ref\<T\>](../atomic_ref.md)
