# Sgcl::AtomicRef

```cpp
#include "sgcl/Sgcl/Concurrent/Atomic.h"   // or "sgcl/Sgcl/Sgcl.h"

namespace Sgcl {
    template<class T>
    class AtomicRef<Ptr<T>>;
}
```

The same class in the `sgcl` interface: [atomic_ref](../../concurrent/atomic_ref.md).

`AtomicRef<Ptr<T>>` is `std::atomic_ref` for a [`Ptr`](../Core/Ptr.md): the operations of [`Atomic<Ptr<T>>`](Atomic.md) (`Load`, `Store`, `Exchange`, `CompareExchangeWeak`, `CompareExchange`, `Wait`, `Notify`, with a `MemoryOrder`) applied to a plain `Ptr` that lives somewhere already: a member of a node, an element of a `List<Ptr<T>>`, a local. The word of a `Ptr` is a `std::atomic` of a pointer in any case, so nothing changes in the pointer's layout; the `AtomicRef` is a reference to it and the operations are the atomic ones, with the hazard pointer of `Atomic::Load` and the same freedom from ABA. A [`RootPtr`](../Core/RootPtr.md) converts to the `Ptr` it holds its object by (the word of its cell, in a managed block), so `AtomicRef a(root)` (deduced, as from a `Ptr`) is the atomic of a root that lives anywhere: a global holding an object that other threads share and that is replaced at run time is a `RootPtr` under an `AtomicRef`. The `Ptr` (or the `RootPtr`) must not be moved or destroyed while a view of it exists, as with any `atomic_ref`. Only this specialization exists.

## Rules

- The `Ptr` referred to lives where one may (a stack, a managed object), and outlives every `AtomicRef` to it. The `AtomicRef` itself is a plain reference and may live anywhere, like `std::atomic_ref`.
- As with `std::atomic_ref`, while any `AtomicRef` to a `Ptr` exists, the pointer is accessed only through `AtomicRef`s: a plain copy or assignment of the pointer at the same time is a data race in the program's terms (the word itself is atomic, so it is never torn, and the collector is correct under any interleaving: [The rules](../../core/README.md#the-rules), 6).
- Copyable (the copy refers to the same pointer), not assignable.
- Every operation is lock-free (`IsAlwaysLockFree`) and may be called from any thread.
- In a destructor the referred `Ptr` is a member of a dying object: its target may be dying in the same sweep, so a destructor does not `Load()` it ([The rules](../../core/README.md#the-rules), 5).

## Members

### ValueType, InnerType

```cpp
using ValueType = Ptr<T>;
using InnerType = sgcl::atomic_ref<sgcl::tracked_ptr<T>>;
```

### Constructors

```cpp
explicit AtomicRef(Ptr<T>& p) noexcept;
explicit AtomicRef(RootPtr<T>& r) noexcept;
AtomicRef(const AtomicRef& a) noexcept;
AtomicRef& operator=(const AtomicRef&) = delete;
```

A reference to `p`, or to the word `r` holds its object by; a copy refers to the same pointer. Not assignable: an `AtomicRef` refers to one pointer for its life.

```cpp
struct Node { Ptr<Node> next; };
Ptr node = Make<Node>();
AtomicRef next(node->next);     // AtomicRef<Ptr<Node>>, deduced
AtomicRef same = next;          // the same word
next.Store(node);
assert(same.Load() == node);
```

### Ref

```cpp
Ptr<T>& Ref() const noexcept;
```

The `Ptr` referred to: for a `RootPtr` the word it holds its object by.

### operator=, operator ValueType

```cpp
std::nullptr_t operator=(std::nullptr_t) noexcept;
void operator=(UniquePtr<T>&& p) noexcept;
Ptr<T> operator=(Ptr<T> p) noexcept;
operator Ptr<T>() const noexcept;
```

`a = p` is `a.Store(p)` with `SeqCst`; `Ptr<T> p = a` is `a.Load()`.

```cpp
Ptr<int> word;
AtomicRef a(word);
a = Make<int>(1);     // a store
Ptr<int> p = a;       // a load
a = nullptr;
assert(*p == 1 && !a.Load());
```

### IsAlwaysLockFree, RequiredAlignment, IsLockFree

```cpp
static constexpr bool IsAlwaysLockFree;
static constexpr size_t RequiredAlignment;
bool IsLockFree() const noexcept;
```

Lock-free on every platform the library supports; every `Ptr` has the required alignment, since its word is that atomic.

### Load

```cpp
Ptr<T> Load(MemoryOrder m = SeqCst) const noexcept;
```

The pointer, as a `Ptr` that holds the object: the word read twice around a hazard pointer published on the calling thread's record, so that the collector cannot reclaim the object between the read and the hold. The order is at least `Acquire`: `Relaxed` is raised to it.

```cpp
Ptr word = Make<int>(1);
Ptr p = AtomicRef(word).Load(Acquire);       // the 1, held
assert(*p == 1);
```

### Store

```cpp
void Store(std::nullptr_t, MemoryOrder m = SeqCst) noexcept;
void Store(UniquePtr<T>&& p, MemoryOrder m = SeqCst) noexcept;
void Store(Ptr<T> p, MemoryOrder m = SeqCst) noexcept;
```

Replaces the pointer: with null, with the object of a `UniquePtr` (released to the collector), or with a copy of a `Ptr`. The old object lives on for whoever holds it. The store carries the write barrier.

```cpp
Ptr<int> word;
AtomicRef(word).Store(Make<int>(1));       // from a UniquePtr
Ptr two = Make<int>(2);
AtomicRef(word).Store(two, Release);
AtomicRef(word).Store(nullptr);
```

### Exchange

```cpp
Ptr<T> Exchange(Ptr<T> p, MemoryOrder m = SeqCst) noexcept;
Ptr<T> Exchange(std::nullptr_t, MemoryOrder m = SeqCst) noexcept;
```

Replaces the pointer and returns the old one, held.

### CompareExchange, CompareExchangeWeak

```cpp
bool CompareExchange(Ptr<T>& e, std::nullptr_t, MemoryOrder m = SeqCst) noexcept;
bool CompareExchange(Ptr<T>& e, Ptr<T> n, MemoryOrder m = SeqCst) noexcept;
bool CompareExchange(Ptr<T>& e, Ptr<T> n, MemoryOrder s, MemoryOrder f) noexcept;

bool CompareExchangeWeak(Ptr<T>& e, std::nullptr_t, MemoryOrder m = SeqCst) noexcept;
bool CompareExchangeWeak(Ptr<T>& e, Ptr<T> n, MemoryOrder m = SeqCst) noexcept;
bool CompareExchangeWeak(Ptr<T>& e, Ptr<T> n, MemoryOrder s, MemoryOrder f) noexcept;
```

The compare-exchange of `std::atomic_ref` (`CompareExchange` the strong one): when the word equals `e`, it is replaced by `n` (or null) and `true` is returned; otherwise `e` is set to the current value, loaded with `Acquire` and held, and `false` is returned. The `Weak` form may fail spuriously and belongs in a loop. With one order `m`, the failure order is derived from it as `std::atomic` does; with two, `s` is the order of the success and `f` of the failure. No ABA: the object `e` holds cannot be reused while `e` holds it.

```cpp
struct Item { int value; Ptr<Item> next; };
struct Pile { Ptr<Item> head; };     // a plain member, used atomically through AtomicRef
Ptr stack = Make<Pile>();

AtomicRef head(stack->head);
Ptr item = Make<Item>(1);
item->next = head.Load();
while (!head.CompareExchangeWeak(item->next, item)) {}   // push

Ptr top = head.Load();
while (top && !head.CompareExchangeWeak(top, top->next)) {}   // pop
assert(top && top->value == 1 && !head.Load());
```

### Wait, NotifyOne, NotifyAll

```cpp
void Wait(std::nullptr_t, MemoryOrder m = SeqCst) const noexcept;
void Wait(Ptr<T> p, MemoryOrder m = SeqCst) const noexcept;
void NotifyOne() noexcept;
void NotifyAll() noexcept;
```

The waiting of `std::atomic_ref`: `Wait(p)` blocks while the word equals `p` (or null), `NotifyOne` and `NotifyAll` wake the threads blocked in `Wait` after a store.

```cpp
struct Slot { Ptr<int> value; };
Ptr slot = Make<Slot>();
Thread producer([&slot] {                          // a reference: the Ptr stays on this frame
    AtomicRef(slot->value).Store(Make<int>(1));
    AtomicRef(slot->value).NotifyOne();
});
AtomicRef(slot->value).Wait(nullptr);          // until the slot is not null
assert(*AtomicRef(slot->value).Load() == 1);
producer.Join();
```

### Inner

```cpp
InnerType& Inner() noexcept;
const InnerType& Inner() const noexcept;
```

The reference inside, as its own type.

### Deduction guides

```cpp
template<class T> AtomicRef(Ptr<T>&) -> AtomicRef<Ptr<T>>;
template<class T> AtomicRef(RootPtr<T>&) -> AtomicRef<Ptr<T>>;
```

`AtomicRef(p)` for a `Ptr<T> p` is an `AtomicRef<Ptr<T>>`; for a `RootPtr<T> r` the same, over the word `r` holds its object by.

## Example

```cpp
#include "sgcl/Sgcl/Sgcl.h"
#include <cassert>
#include <iostream>

// A table of slots that several threads fill: the slots are plain Ptrs in
// a managed buffer, and each claim is a compare-exchange through an
// AtomicRef. The first thread to claim a slot wins; the losers see its
// item and move on. No slot is ever a torn pointer, and an item that lost
// the race is garbage the collector reclaims.
struct Item {
    explicit Item(int owner) : owner(owner) {}
    int owner;
};

int main() {
    List<Ptr<Item>> slots(64);       // 64 null slots in a managed buffer

    List<Thread> workers;
    for (int t : Range(4)) {
        workers.Emplace([&slots, t] {              // a reference: the List stays in main's frame
            for (size_t i : Range(slots.Count())) {
                Ptr<Item> expected;            // null: the slot is free
                Ptr mine = Make<Item>(t);
                AtomicRef slot(slots[i]);
                if (!slot.CompareExchange(expected, mine)) {
                    assert(expected);                   // somebody else's item, now held by `expected`
                }
            }
        });
    }
    for (auto& w : workers) {
        w.Join();
    }

    int counts[4] = {};
    for (const auto& s : slots) {
        assert(s);                                      // every slot claimed exactly once
        ++counts[s->owner];
    }
    std::cout << counts[0] << ' ' << counts[1] << ' ' << counts[2] << ' ' << counts[3] << '\n';

    Collector::Collect(true);     // optional, for the demonstration only: the collector runs its cycles by itself
    return 0;
}
```

The output of one run (the split between the workers varies):

```
2 62 0 0
```

## See also

- [Atomic](Atomic.md): the same operations on a `Ptr` declared atomic
- [Ptr](../Core/Ptr.md), [RootPtr](../Core/RootPtr.md), [UniquePtr](../Core/UniquePtr.md), [Make](../Core/Make.md), [List](../Containers/List.md)
- README: [The classes](../../core/README.md#the-classes), [The rules](../../core/README.md#the-rules), [Threads](../../async/README.md#threads)
