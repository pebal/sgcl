# sgcl::atomic_ref

```cpp
#include "sgcl/concurrent/atomic_ref.h"   // or "sgcl/sgcl.h"

namespace sgcl {
    template<class T>
    class atomic_ref<tracked_ptr<T>>;
    template<class T>
    class atomic_ref<sgcl::tracked_ptr<T>>;
}
```

`atomic_ref<tracked_ptr<T>>` is `std::atomic_ref` for a [`tracked_ptr`](../core/tracked_ptr.md): the operations of [`atomic<tracked_ptr<T>>`](atomic.md) (`load`, `store`, `exchange`, `compare_exchange_weak`, `compare_exchange_strong`, `wait`, `notify`, with a `std::memory_order`) applied to a plain `tracked_ptr` that lives somewhere already: a member of a node, an element of an `sgcl::vector<tracked_ptr<T>>`, a local. The word of a `tracked_ptr` is a `std::atomic` of a pointer in any case, so nothing changes in the pointer's layout; the `atomic_ref` is a reference to it and the operations are the atomic ones, with the hazard pointer of `atomic::load` and the same freedom from ABA. A [`root_ptr`](../core/root_ptr.md) converts to the `tracked_ptr` it holds its object by (the word of its cell, in a managed block), so `atomic_ref a(root)` (deduced, as from a `tracked_ptr`) is the atomic of a root that lives anywhere: a global holding an object that other threads share and that is replaced at run time is a `root_ptr` under an `atomic_ref`. The `tracked_ptr` (or the `root_ptr`) must not be moved or destroyed while a view of it exists, as with any `atomic_ref`. Only this specialization exists.

## Rules

- The `tracked_ptr` referred to lives where one may (a stack, a managed object), and outlives every `atomic_ref` to it. The `atomic_ref` itself is a plain reference and may live anywhere, like `std::atomic_ref`.
- As with `std::atomic_ref`, while any `atomic_ref` to a `tracked_ptr` exists, the pointer is accessed only through `atomic_ref`s: a plain copy or assignment of the pointer at the same time is a data race in the program's terms (the word itself is atomic, so it is never torn, and the collector is correct under any interleaving: [The rules](../core/README.md#the-rules), 6).
- Copyable (the copy refers to the same pointer), not assignable.
- Every operation is lock-free (`is_always_lock_free`) and may be called from any thread.
- In a destructor the referred `tracked_ptr` is a member of a dying object: its target may be dying in the same sweep, so a destructor does not `load()` it ([The rules](../core/README.md#the-rules), 5).

## Members

### value_type

```cpp
using value_type = tracked_ptr<T>;
```

### Constructors

```cpp
explicit atomic_ref(value_type& p) noexcept;
atomic_ref(const atomic_ref& a) noexcept;
atomic_ref& operator=(const atomic_ref&) = delete;
```

A reference to `p`; a copy refers to the same pointer. Not assignable: an `atomic_ref` refers to one pointer for its life.

```cpp
struct Node { tracked_ptr<Node> next; };
tracked_ptr node = make_tracked<Node>();
atomic_ref next(node->next);     // atomic_ref<tracked_ptr<Node>>, deduced
atomic_ref same = next;          // the same word
next.store(node);
assert(same.load() == node);
```

### ref

```cpp
tracked_ptr<T>& ref;
```

The `tracked_ptr` referred to, a public member: for a `root_ptr` the word it holds its object by.

### operator=, operator value_type

```cpp
std::nullptr_t operator=(std::nullptr_t) noexcept;
void operator=(unique_ptr<T>&& p) noexcept;
value_type operator=(tracked_ptr<T> p) noexcept;
operator value_type() const noexcept;
```

`a = p` is `a.store(p)` with `std::memory_order_seq_cst`; `tracked_ptr<T> p = a` is `a.load()`.

```cpp
tracked_ptr<int> word;
atomic_ref a(word);
a = make_tracked<int>(1);     // a store
tracked_ptr<int> p = a;       // a load
a = nullptr;
assert(*p == 1 && !a.load());
```

### is_always_lock_free, required_alignment, is_lock_free

```cpp
static constexpr bool is_always_lock_free;        // atomic<void*>::is_always_lock_free
static constexpr std::size_t required_alignment;  // alignof(atomic<void*>)
bool is_lock_free() const noexcept;
```

Lock-free on every platform the library supports; every `tracked_ptr` has the required alignment, since its word is that atomic.

### load

```cpp
value_type load(const std::memory_order m = std::memory_order_seq_cst) const noexcept;
```

The pointer, as a `tracked_ptr` that holds the object: the word read twice around a hazard pointer published on the calling thread's record, so that the collector cannot reclaim the object between the read and the hold. The order is at least `acquire`: `relaxed` and `consume` are raised to it.

```cpp
tracked_ptr word = make_tracked<int>(1);
tracked_ptr p = atomic_ref(word).load(std::memory_order_acquire);       // the 1, held
assert(*p == 1);
```

### store

```cpp
void store(std::nullptr_t, const std::memory_order m = std::memory_order_seq_cst) noexcept;
void store(unique_ptr<T>&& p, const std::memory_order m = std::memory_order_seq_cst) noexcept;
void store(tracked_ptr<T> p, const std::memory_order m = std::memory_order_seq_cst) noexcept;
```

Replaces the pointer: with null, with the object of a `unique_ptr` (released to the collector), or with a copy of a `tracked_ptr`. The old object lives on for whoever holds it. The store carries the write barrier.

```cpp
tracked_ptr<int> word;
atomic_ref(word).store(make_tracked<int>(1));       // from a unique_ptr
tracked_ptr two = make_tracked<int>(2);
atomic_ref(word).store(two, std::memory_order_release);
atomic_ref(word).store(nullptr);
```

### compare_exchange_strong, compare_exchange_weak

```cpp
bool compare_exchange_strong(tracked_ptr<T>& e, std::nullptr_t, const std::memory_order m = std::memory_order_seq_cst) noexcept;
bool compare_exchange_strong(tracked_ptr<T>& e, tracked_ptr<T> n, const std::memory_order m = std::memory_order_seq_cst) noexcept;
bool compare_exchange_strong(tracked_ptr<T>& e, std::nullptr_t, const std::memory_order s, const std::memory_order f) noexcept;
bool compare_exchange_strong(tracked_ptr<T>& e, tracked_ptr<T> n, const std::memory_order s, const std::memory_order f) noexcept;

bool compare_exchange_weak(tracked_ptr<T>& e, std::nullptr_t, const std::memory_order m = std::memory_order_seq_cst) noexcept;
bool compare_exchange_weak(tracked_ptr<T>& e, tracked_ptr<T> n, const std::memory_order m = std::memory_order_seq_cst) noexcept;
bool compare_exchange_weak(tracked_ptr<T>& e, std::nullptr_t, const std::memory_order s, const std::memory_order f) noexcept;
bool compare_exchange_weak(tracked_ptr<T>& e, tracked_ptr<T> n, const std::memory_order s, const std::memory_order f) noexcept;
```

The compare-exchange of `std::atomic_ref`: when the word equals `e`, it is replaced by `n` (or null) and `true` is returned; otherwise `e` is set to the current value, loaded with `acquire` and held, and `false` is returned. The `weak` form may fail spuriously and belongs in a loop. With one order `m`, the failure order is derived from it as `std::atomic` does; with two, `s` is the order of the success and `f` of the failure. No ABA: the object `e` holds cannot be reused while `e` holds it.

```cpp
struct Item { int value; tracked_ptr<Item> next; };
struct Pile { tracked_ptr<Item> head; };     // a plain member, used atomically through atomic_ref
tracked_ptr stack = make_tracked<Pile>();

atomic_ref head(stack->head);
tracked_ptr item = make_tracked<Item>(1);
item->next = head.load();
while (!head.compare_exchange_weak(item->next, item)) {}   // push

tracked_ptr top = head.load();
while (top && !head.compare_exchange_weak(top, top->next)) {}   // pop
assert(top && top->value == 1 && !head.load());
```

### wait, notify_one, notify_all

```cpp
void wait(std::nullptr_t, std::memory_order m = std::memory_order_seq_cst) const noexcept;
void wait(tracked_ptr<T> p, std::memory_order m = std::memory_order_seq_cst) const noexcept;
void notify_one() noexcept;
void notify_all() noexcept;
```

The waiting of `std::atomic_ref`: `wait(p)` blocks while the word equals `p` (or null), `notify_one` and `notify_all` wake the threads blocked in `wait` after a store.

```cpp
struct Slot { tracked_ptr<int> value; };
tracked_ptr slot = make_tracked<Slot>();
thread producer([&slot] {                      // a reference: the tracked_ptr stays on this frame
    atomic_ref(slot->value).store(make_tracked<int>(1));
    atomic_ref(slot->value).notify_one();
});
atomic_ref(slot->value).wait(nullptr);          // until the slot is not null
assert(*atomic_ref(slot->value).load() == 1);
producer.join();
```

### Deduction guides

```cpp
template<class T> atomic_ref(tracked_ptr<T>) -> atomic_ref<tracked_ptr<T>>;
template<class T> atomic_ref(root_ptr<T>) -> atomic_ref<tracked_ptr<T>>;
```

`sgcl::atomic_ref(p)` for a `tracked_ptr<T> p` is an `atomic_ref<tracked_ptr<T>>`; for a `root_ptr<T> r` the same, over the word `r` holds its object by.

## Example

```cpp
#include "sgcl/sgcl.h"
#include <cassert>
#include <iostream>

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
    vector<tracked_ptr<Item>> slots(64);       // 64 null slots in a managed buffer

    vector<thread> workers;
    for (int t : range(4)) {
        workers.emplace_back([&slots, t] {              // a reference: the vector stays in main's frame
            for (size_t i : range(slots.size())) {
                tracked_ptr<Item> expected;         // null: the slot is free
                tracked_ptr mine = make_tracked<Item>(t);
                atomic_ref slot(slots[i]);
                if (!slot.compare_exchange_strong(expected, mine)) {
                    assert(expected);                   // somebody else's item, now held by `expected`
                }
            }
        });
    }
    for (auto& w : workers) {
        w.join();
    }

    int counts[4] = {};
    for (const auto& s : slots) {
        assert(s);                                      // every slot claimed exactly once
        ++counts[s->owner];
    }
    std::cout << counts[0] << ' ' << counts[1] << ' ' << counts[2] << ' ' << counts[3] << '\n';

    collector::force_collect(true);     // optional, for the demonstration only: the collector runs its cycles by itself
    return 0;
}
```

The output of one run (the split between the workers varies):

```
1 0 0 63
```

## See also

- [atomic](atomic.md): the same operations on a `tracked_ptr` declared atomic
- [tracked_ptr](../core/tracked_ptr.md), [unique_ptr](../core/unique_ptr.md), [make_tracked](../core/make_tracked.md), [vector](../containers/vector.md)
- README: [The classes](../core/README.md#the-classes), [The rules](../core/README.md#the-rules), [Threads](../async/README.md#threads)
- `examples/lock_free_stack.cpp`, `examples/threads.cpp`
