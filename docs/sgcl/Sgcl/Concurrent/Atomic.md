# Sgcl::Atomic

```cpp
#include "sgcl/Sgcl/Concurrent/Atomic.h"   // or "sgcl/Sgcl/Sgcl.h"

namespace Sgcl {
    template<class T>
    class Atomic;   // std::atomic<T>, for every T but the two below
    template<class T>
    class Atomic<Ptr<T>>;
    template<>
    class Atomic<String>;
    using MemoryOrder = std::memory_order;
    inline constexpr MemoryOrder Relaxed, Acquire, Release, AcqRel, SeqCst;
}
```

The same class in the `sgcl` interface: [atomic](../../concurrent/atomic.md).

`Atomic<Ptr<T>>` is `std::atomic` for a [`Ptr`](../Core/Ptr.md): a word that several threads read and write without a lock, with `Load`, `Store`, `Exchange`, `CompareExchangeWeak`, `CompareExchange`, `Wait` and `Notify` taking a `MemoryOrder`. It is the answer to rule 6: a `Ptr` written by one thread and read by another needs `Atomic`, `AtomicRef` or the program's own synchronization. A global shared pointer is a [`RootPtr`](../Core/RootPtr.md) with an [`AtomicRef`](AtomicRef.md) over it: the `RootPtr` holds its object by a `Ptr` in a managed block, and `AtomicRef` binds that word (`static RootPtr<Config> current; AtomicRef a(current);`). The second specialization is the atomic of a [`String`](../Core/String.md) (below). For every other type, `Atomic<T>` is `std::atomic<T>` under the interface's names, so that a program names one atomic for its flags, its counters and its pointers alike; a value shared whole between threads, an object rather than a word, is a [`CopyOnWrite`](CopyOnWrite.md).

The difference from `std::atomic<std::shared_ptr<T>>` is that it is lock-free, one word, and safe against reuse: a `Load()` publishes a hazard pointer for the length of the load, so that the collector cannot reclaim the object between the read of the word and the construction of the `Ptr` that holds it, and once held the object cannot be reclaimed at all. A compare-exchange therefore has no ABA problem: a node is never freed and reused while any thread holds a `Ptr` to it, so the address it compares against is the node it means. A lock-free stack or queue needs no hazard pointers or epochs of its own ("Lock-free stack" in the [Benchmarks](../../concurrent/benchmarks.md#lock-free-stack)).

## Rules

- An `Atomic<Ptr<T>>` holds a `Ptr`, so it lives where one may: on a stack or inside a managed object, never in `new`/`malloc` memory, a `std` container or a global. A global shared pointer is a `RootPtr<T>` under an `AtomicRef` ([AtomicRef](AtomicRef.md)), or a `UniquePtr` to a managed object holding the atomic: `static UniquePtr current = Make<Atomic<Ptr<Config>>>();` ([The rules](../../core/README.md#the-rules), 1 and 6).
- It is neither copyable nor movable, like `std::atomic`.
- Every operation is lock-free (`IsAlwaysLockFree`) and may be called from any thread. A `Load()` costs the read of the word twice around a hazard store; a `Store()` or a compare-exchange the atomic operation and the write barrier.
- In a destructor an `Atomic` member is a `Ptr` member without `IfAlive()`: its target may be dying in the same sweep, so a destructor does not read it ([The rules](../../core/README.md#the-rules), 5).

## Members

### ValueType, InnerType

```cpp
using ValueType = Ptr<T>;
using InnerType = sgcl::atomic<sgcl::tracked_ptr<T>>;
```

### Constructors

```cpp
Atomic() noexcept;
Atomic(std::nullptr_t) noexcept;
Atomic(UniquePtr<T>&& p) noexcept;
Atomic(Ptr<T> p) noexcept;
Atomic(const Atomic&) = delete;
Atomic& operator=(const Atomic&) = delete;
```

Null by default or from `nullptr`; from a `UniquePtr<T>&&`, whose object is released to the collector; or from a `Ptr<T>`. Not copyable, not movable.

```cpp
struct Node { int value = 0; };
Atomic<Ptr<Node>> empty;                             // null
Atomic<Ptr<Node>> made(Make<Node>());       // from a UniquePtr
Ptr node = Make<Node>();
Atomic<Ptr<Node>> shared(node);                      // from a Ptr
```

### operator=, operator ValueType

```cpp
std::nullptr_t operator=(std::nullptr_t) noexcept;
void operator=(UniquePtr<T>&& p) noexcept;
Ptr<T> operator=(Ptr<T> p) noexcept;
operator Ptr<T>() const noexcept;
```

`a = p` is `a.Store(p)` with `SeqCst`; `Ptr<T> p = a` is `a.Load()`.

```cpp
Atomic<Ptr<int>> a;
a = Make<int>(1);             // a store
Ptr<int> p = a;               // a load, SeqCst
a = nullptr;
assert(*p == 1 && !a.Load());
```

### IsAlwaysLockFree, IsLockFree

```cpp
static constexpr bool IsAlwaysLockFree;
bool IsLockFree() const noexcept;
```

True on every platform the library supports: the word is a `std::atomic` of a pointer.

### Load

```cpp
Ptr<T> Load(MemoryOrder m = SeqCst) const noexcept;
```

The pointer, as a `Ptr` that holds the object. The word is read, a hazard pointer to it published on the calling thread's record, and the word read again until the two reads agree; the `Ptr` is then constructed and the hazard cleared. The collector reads the hazards before it reclaims anything, so the object cannot be freed between the read and the hold. The order is at least `Acquire`: `Relaxed` is raised to it.

```cpp
Atomic<Ptr<int>> a(Make<int>(1));
Ptr p = a.Load(Acquire);     // Ptr<int>: the 1, held
assert(*p == 1);
```

### Store

```cpp
void Store(std::nullptr_t, MemoryOrder m = SeqCst) noexcept;
void Store(UniquePtr<T>&& p, MemoryOrder m = SeqCst) noexcept;
void Store(Ptr<T> p, MemoryOrder m = SeqCst) noexcept;
```

Replaces the pointer: with null, with the object of a `UniquePtr` (released to the collector), or with a copy of a `Ptr`, taken by value so that its target is held for the length of the call. The old object lives on for whoever holds it. The store carries the write barrier.

```cpp
Atomic<Ptr<int>> a;
a.Store(Make<int>(1));                 // from a UniquePtr
Ptr two = Make<int>(2);
a.Store(two, Release);                 // from a Ptr
a.Store(nullptr);
```

### Exchange

```cpp
Ptr<T> Exchange(Ptr<T> p, MemoryOrder m = SeqCst) noexcept;
Ptr<T> Exchange(std::nullptr_t, MemoryOrder m = SeqCst) noexcept;
```

Replaces the pointer and returns the old one, held: the old object is under the hazard pointer from before the exchange until the returned `Ptr` holds it, so it cannot be reclaimed in between. The exchange carries the write barrier.

```cpp
struct Node { Ptr<Node> next; };
Atomic<Ptr<Node>> head;
Ptr all = head.Exchange(nullptr);     // the whole list taken, in one step
```

### CompareExchange, CompareExchangeWeak

```cpp
bool CompareExchange(Ptr<T>& e, std::nullptr_t, MemoryOrder m = SeqCst) noexcept;
bool CompareExchange(Ptr<T>& e, Ptr<T> n, MemoryOrder m = SeqCst) noexcept;
bool CompareExchange(Ptr<T>& e, Ptr<T> n, MemoryOrder s, MemoryOrder f) noexcept;

bool CompareExchangeWeak(Ptr<T>& e, std::nullptr_t, MemoryOrder m = SeqCst) noexcept;
bool CompareExchangeWeak(Ptr<T>& e, Ptr<T> n, MemoryOrder m = SeqCst) noexcept;
bool CompareExchangeWeak(Ptr<T>& e, Ptr<T> n, MemoryOrder s, MemoryOrder f) noexcept;
```

The compare-exchange of `std::atomic` (`CompareExchange` the strong one): when the word equals `e`, it is replaced by `n` (or null) and `true` is returned; otherwise `e` is set to the current value, loaded with `Acquire` and held, and `false` is returned. The `Weak` form may fail spuriously and belongs in a loop. With one order `m`, the failure order is derived from it as `std::atomic` does; with two, `s` is the order of the success and `f` of the failure. No ABA: the object `e` holds cannot be reused while `e` holds it, so an equal address is the same object.

```cpp
struct Item { int value; Ptr<Item> next; };
Atomic<Ptr<Item>> head;

// push: the new item's next is the head as last seen, until the CAS lands
Ptr item = Make<Item>(1);
item->next = head.Load();
while (!head.CompareExchangeWeak(item->next, item)) {}

// pop: the head replaced by its next, or null
Ptr top = head.Load();
while (top && !head.CompareExchangeWeak(top, top->next)) {}
assert(top && top->value == 1 && !head.Load());
```

### Wait, NotifyOne, NotifyAll

```cpp
void Wait(std::nullptr_t, MemoryOrder m = SeqCst) const noexcept;
void Wait(Ptr<T> p, MemoryOrder m = SeqCst) const noexcept;
void NotifyOne() noexcept;
void NotifyAll() noexcept;
```

The waiting of `std::atomic`: `Wait(p)` blocks while the word equals `p` (or null), `NotifyOne` and `NotifyAll` wake the threads blocked in `Wait` after a store.

```cpp
Atomic<Ptr<int>> slot;
Thread producer([&slot] {         // the lambda captures a reference: no Ptr copied to the heap
    slot.Store(Make<int>(1));
    slot.NotifyOne();
});
slot.Wait(nullptr);                    // until the slot is not null
assert(*slot.Load() == 1);
producer.Join();
```

### Inner

```cpp
InnerType& Inner() noexcept;
const InnerType& Inner() const noexcept;
```

The atomic word inside, as its own type.

### Atomic\<T\> for a plain T

```cpp
template<class T>
class Atomic {
public:
    using ValueType = T;
    using InnerType = sgcl::atomic<T>;
    static constexpr bool IsAlwaysLockFree;
    Atomic() noexcept;
    Atomic(T v) noexcept;
    T operator=(T v) noexcept;
    operator T() const noexcept;
    bool IsLockFree() const noexcept;
    T Load(MemoryOrder m = SeqCst) const noexcept;
    void Store(T v, MemoryOrder m = SeqCst) noexcept;
    T Exchange(T v, MemoryOrder m = SeqCst) noexcept;
    bool CompareExchange(T& expected, T desired, MemoryOrder m = SeqCst) noexcept;
    bool CompareExchange(T& expected, T desired, MemoryOrder success, MemoryOrder failure) noexcept;
    bool CompareExchangeWeak(T& expected, T desired, MemoryOrder m = SeqCst) noexcept;
    bool CompareExchangeWeak(T& expected, T desired, MemoryOrder success, MemoryOrder failure) noexcept;
    T FetchAdd(T v, MemoryOrder m = SeqCst) noexcept;   // an integral or a pointer; FetchSub too
    T FetchAnd(T v, MemoryOrder m = SeqCst) noexcept;   // an integral; FetchOr, FetchXor too
    T operator++() noexcept;  T operator++(int) noexcept;  T operator--() noexcept;  T operator--(int) noexcept;
    T operator+=(T v) noexcept;  T operator-=(T v) noexcept;
    void Wait(T old, MemoryOrder m = SeqCst) const noexcept;
    void NotifyOne() noexcept;
    void NotifyAll() noexcept;
    InnerType& Inner() noexcept;
};
```

`std::atomic<T>` for a flag, a counter, a raw pointer, with the names of the interface: the same operations, the same orders, the same cost. It lives anywhere a `std::atomic` does.

```cpp
Atomic counter = 0;
counter += 5;
assert(counter.FetchAdd(1) == 5 && counter.Load() == 6 && ++counter == 7);
Atomic done = false;
done = true;
assert(done.Load());
```

### Atomic\<String\>

```cpp
template<>
class Atomic<String> {
public:
    using ValueType = String;
    using InnerType = sgcl::atomic<sgcl::string>;
    Atomic() noexcept;
    Atomic(const String& s) noexcept;
    String Load(MemoryOrder m = SeqCst) const noexcept;
    operator String() const noexcept;
    void Store(const String& s, MemoryOrder m = SeqCst) noexcept;
    String operator=(const String& s) noexcept;
    String Exchange(const String& s, MemoryOrder m = SeqCst) noexcept;
    bool CompareExchange(String& expected, const String& desired, MemoryOrder m = SeqCst) noexcept;
    bool CompareExchange(String& expected, const String& desired, MemoryOrder success, MemoryOrder failure) noexcept;
    bool CompareExchangeWeak(String& expected, const String& desired, MemoryOrder m = SeqCst) noexcept;
    void Wait(const String& s, MemoryOrder m = SeqCst) const noexcept;
    void NotifyOne() noexcept;
    void NotifyAll() noexcept;
    InnerType& Inner() noexcept;
};
```

A `String` is one word to an object never modified, so the atomic of a string is the atomic of that word: the operations above with a string on the outside, one word in size. A load is one atomic load (with the hazard pointer of every atomic load) and the string it returns is the object as it was, whatever is stored meanwhile; a store is the store of the object the caller has already made; nothing is copied and nothing allocated beyond the strings themselves. The compare-exchanges compare identity, the object, as a compare-exchange on a word does: two strings of the same characters made apart are two objects, and the expected one must be the one loaded or stored from here, not one equal to it (`String("a")` is never the string that is there); an exchange for a change of contents loads, decides, and exchanges against what it loaded. `Atomic<String>` lives where a `String` does, on a stack or inside a managed object.

```cpp
static Atomic<String> currentHost = String("localhost");   // a global, read by every thread

String host = currentHost.Load();                 // one load: the string as it was
currentHost = String("db.internal");              // a store: the old one dies with its last reader
String seen = currentHost.Load();
if (!currentHost.CompareExchange(seen, String("db2.internal"))) {
    // someone stored since: seen is what is there now
}
```

## Example

```cpp
#include "sgcl/Sgcl/Sgcl.h"
#include <cassert>
#include <iostream>

// A configuration replaced at run time under readers on other threads: the
// readers load it, the writer stores a new one, and the old one is
// collected when the last reader drops it. No lock, no count.
struct Config {
    explicit Config(int version) : version(version) {}
    int version;
};

int main() {
    Atomic current = Make<Config>(0);   // on main's stack, where a Ptr may live

    List<Thread> readers;
    for (int t : Range(4)) {
        readers.Emplace([&current] {                // by reference: main's frame outlives the threads it joins
            int last = -1;
            for (int i : Range(100000)) {
                Ptr config = current.Load();                 // held: cannot be reclaimed under this thread
                assert(config->version >= last);         // versions only go up
                last = config->version;
            }
        });
    }

    for (int version : Range(1, 101)) {
        current.Store(Make<Config>(version));    // the old Config lives on for its readers
    }
    for (auto& r : readers) {
        r.Join();
    }
    std::cout << "final version " << current.Load()->version << '\n';   // 100

    Collector::Collect(true);     // optional, for the demonstration only: the collector runs its cycles by itself
    return 0;
}
```

The output:

```
final version 100
```

## See also

- [AtomicRef](AtomicRef.md): the same operations on a `Ptr` that is not declared atomic
- [Ptr](../Core/Ptr.md), [UniquePtr](../Core/UniquePtr.md), [Make](../Core/Make.md)
- README: [The classes](../../core/README.md#the-classes), [The rules](../../core/README.md#the-rules), [Threads](../../async/README.md#threads), [Benchmarks: Lock-free stack](../../concurrent/benchmarks.md#lock-free-stack)
