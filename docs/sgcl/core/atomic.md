# sgcl::atomic

```cpp
#include "sgcl/core/atomic.h"   // or "sgcl/sgcl.h"

namespace sgcl {
    template<class T>
    class atomic;   // std::atomic<T>, for every T but the three below
    template<class T>
    class atomic<tracked_ptr<T>>;
    template<class T>
    class atomic<sgcl::tracked_ptr<T>>;
    template<class CharT, class Traits>
    class atomic<basic_string<CharT, Traits>>;
}
```

`atomic<tracked_ptr<T>>` is `std::atomic` for a [`tracked_ptr`](tracked_ptr.md): a word that several threads read and write without a lock, with `load`, `store`, `exchange`, `compare_exchange_weak`, `compare_exchange_strong`, `wait` and `notify` taking a `std::memory_order`. It is the answer to rule 6: a `tracked_ptr` written by one thread and read by another needs `atomic`, `atomic_ref` or the program's own synchronization. A global shared pointer is a [`root_ptr`](root_ptr.md) with an [`atomic_ref`](atomic_ref.md) over it: the `root_ptr` holds its object by a `tracked_ptr` in a managed block, and `atomic_ref` binds that word (`static sgcl::root_ptr<Config> current; sgcl::atomic_ref a(current);`). The second specialization is the atomic of a [`string`](string.md) (below). For every other type, `sgcl::atomic<T>` is `std::atomic<T>`, derived from it with its constructors and assignments, so that a program names one atomic for its flags, its counters and its pointers alike; a value shared whole between threads, an object rather than a word, is a [`copy_on_write`](../concurrent/copy_on_write.md).

The difference from `std::atomic<std::shared_ptr<T>>` is that it is lock-free, one word, and safe against reuse: a `load()` publishes a hazard pointer for the length of the load, so that the collector cannot reclaim the object between the read of the word and the construction of the `tracked_ptr` that holds it, and once held the object cannot be reclaimed at all. A compare-exchange therefore has no ABA problem: a node is never freed and reused while any thread holds a `tracked_ptr` to it, so the address it compares against is the node it means. A lock-free stack or queue needs no hazard pointers or epochs of its own (`examples/lock_free_stack.cpp`, and "Lock-free stack" in the [Benchmarks](../concurrent/benchmarks.md#lock-free-stack)).

## Rules

- An `atomic<tracked_ptr<T>>` holds a `tracked_ptr`, so it lives where one may: on a stack or inside a managed object, never in `new`/`malloc` memory, a `std` container or a global. A global shared pointer is a `root_ptr<T>` under an `atomic_ref` ([atomic_ref](atomic_ref.md)), or a `unique_ptr` to a managed object holding the atomic: `static sgcl::unique_ptr current = sgcl::make_tracked<sgcl::atomic<sgcl::tracked_ptr<Config>>>();` ([The rules](README.md#the-rules), 1 and 6).
- It is neither copyable nor movable, like `std::atomic`.
- Every operation is lock-free (`is_always_lock_free`) and may be called from any thread. A `load()` costs the read of the word twice around a hazard store; a `store()` or a compare-exchange the atomic operation and the write barrier.
- In a destructor an `atomic` member is a `tracked_ptr` member without `if_alive()`: its target may be dying in the same sweep, so a destructor does not read it ([The rules](README.md#the-rules), 5).

## Members

### value_type

```cpp
using value_type = tracked_ptr<T>;
```

### Constructors

```cpp
atomic() noexcept;
atomic(std::nullptr_t) noexcept;
atomic(unique_ptr<T>&& p) noexcept;
atomic(tracked_ptr<T> p) noexcept;
atomic(const atomic&) = delete;
atomic& operator=(const atomic&) = delete;
```

Null by default or from `nullptr`; from a `unique_ptr<T>&&`, whose object is released to the collector; or from a `tracked_ptr<T>`. Not copyable, not movable.

```cpp
struct Node { int value = 0; };
atomic<tracked_ptr<Node>> empty;                                 // null
atomic<tracked_ptr<Node>> made(make_tracked<Node>());         // from a unique_ptr
tracked_ptr node = make_tracked<Node>();
atomic<tracked_ptr<Node>> shared(node);                           // from a tracked_ptr
```

### operator=, operator value_type

```cpp
std::nullptr_t operator=(std::nullptr_t) noexcept;
void operator=(unique_ptr<T>&& p) noexcept;
value_type operator=(tracked_ptr<T> p) noexcept;
operator value_type() const noexcept;
```

`a = p` is `a.store(p)` with `std::memory_order_seq_cst`; `tracked_ptr<T> p = a` is `a.load()`.

```cpp
atomic<tracked_ptr<int>> a;
a = make_tracked<int>(1);             // a store
tracked_ptr<int> p = a;               // a load, seq_cst
a = nullptr;
assert(*p == 1 && !a.load());
```

### is_always_lock_free, is_lock_free

```cpp
static constexpr bool is_always_lock_free;   // atomic<void*>::is_always_lock_free
bool is_lock_free() const noexcept;
```

True on every platform the library supports: the word is a `std::atomic` of a pointer.

### load

```cpp
value_type load(const std::memory_order m = std::memory_order_seq_cst) const noexcept;
```

The pointer, as a `tracked_ptr` that holds the object. The word is read, a hazard pointer to it published on the calling thread's record, and the word read again until the two reads agree; the `tracked_ptr` is then constructed and the hazard cleared. The collector reads the hazards before it reclaims anything, so the object cannot be freed between the read and the hold. The order is at least `acquire`: `relaxed` and `consume` are raised to it.

```cpp
atomic<tracked_ptr<int>> a(make_tracked<int>(1));
tracked_ptr p = a.load(std::memory_order_acquire);     // tracked_ptr<int>: the 1, held
assert(*p == 1);
```

### store

```cpp
void store(std::nullptr_t, const std::memory_order m = std::memory_order_seq_cst) noexcept;
void store(unique_ptr<T>&& p, const std::memory_order m = std::memory_order_seq_cst) noexcept;
void store(tracked_ptr<T> p, const std::memory_order m = std::memory_order_seq_cst) noexcept;
```

Replaces the pointer: with null, with the object of a `unique_ptr` (released to the collector), or with a copy of a `tracked_ptr`, taken by value so that its target is held for the length of the call. The old object lives on for whoever holds it. The store carries the write barrier.

```cpp
atomic<tracked_ptr<int>> a;
a.store(make_tracked<int>(1));                         // from a unique_ptr
tracked_ptr two = make_tracked<int>(2);
a.store(two, std::memory_order_release);                   // from a tracked_ptr
a.store(nullptr);
```

### exchange

```cpp
tracked_ptr<T> exchange(tracked_ptr<T> p, const std::memory_order m = std::memory_order_seq_cst) noexcept;
tracked_ptr<T> exchange(std::nullptr_t, const std::memory_order m = std::memory_order_seq_cst) noexcept;
```

Replaces the pointer and returns the old one, held: the old object is under the hazard pointer from before the exchange until the returned `tracked_ptr` holds it, so it cannot be reclaimed in between. The exchange carries the write barrier.

```cpp
atomic<tracked_ptr<Node>> head;
tracked_ptr all = head.exchange(nullptr);            // the whole list taken, in one step
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

The compare-exchange of `std::atomic`: when the word equals `e`, it is replaced by `n` (or null) and `true` is returned; otherwise `e` is set to the current value, loaded with `acquire` and held, and `false` is returned. The `weak` form may fail spuriously and belongs in a loop. With one order `m`, the failure order is derived from it as `std::atomic` does; with two, `s` is the order of the success and `f` of the failure. No ABA: the object `e` holds cannot be reused while `e` holds it, so an equal address is the same object.

```cpp
struct Item { int value; tracked_ptr<Item> next; };
atomic<tracked_ptr<Item>> head;

// push: the new item's next is the head as last seen, until the CAS lands
tracked_ptr item = make_tracked<Item>(1);
item->next = head.load();
while (!head.compare_exchange_weak(item->next, item)) {}

// pop: the head replaced by its next, or null
tracked_ptr top = head.load();
while (top && !head.compare_exchange_weak(top, top->next)) {}
assert(top && top->value == 1 && !head.load());
```

### wait, notify_one, notify_all

```cpp
void wait(std::nullptr_t, std::memory_order m = std::memory_order_seq_cst) const noexcept;
void wait(tracked_ptr<T> p, std::memory_order m = std::memory_order_seq_cst) const noexcept;
void notify_one() noexcept;
void notify_all() noexcept;
```

The waiting of `std::atomic`: `wait(p)` blocks while the word equals `p` (or null), `notify_one` and `notify_all` wake the threads blocked in `wait` after a store.

```cpp
atomic<tracked_ptr<int>> slot;
thread producer([&slot] {         // the lambda captures a reference: no tracked_ptr copied to the heap
    slot.store(make_tracked<int>(1));
    slot.notify_one();
});
slot.wait(nullptr);                    // until the slot is not null
assert(*slot.load() == 1);
producer.join();
```

### atomic<basic_string>

```cpp
template<class CharT, class Traits>
class atomic<basic_string<CharT, Traits>> {
public:
    using value_type = basic_string<CharT, Traits>;
    atomic() noexcept;
    atomic(const value_type& s) noexcept;
    value_type load(std::memory_order m = std::memory_order_seq_cst) const noexcept;
    operator value_type() const noexcept;
    void store(const value_type& s, std::memory_order m = std::memory_order_seq_cst) noexcept;
    value_type operator=(const value_type& s) noexcept;
    value_type exchange(const value_type& s, std::memory_order m = std::memory_order_seq_cst) noexcept;
    bool compare_exchange_strong(value_type& expected, const value_type& desired, std::memory_order m = std::memory_order_seq_cst) noexcept;
    bool compare_exchange_strong(value_type& expected, const value_type& desired, std::memory_order success, std::memory_order failure) noexcept;
    bool compare_exchange_weak(value_type& expected, const value_type& desired, std::memory_order m = std::memory_order_seq_cst) noexcept;
    bool compare_exchange_weak(value_type& expected, const value_type& desired, std::memory_order success, std::memory_order failure) noexcept;
    void wait(const value_type& s, std::memory_order m = std::memory_order_seq_cst) const noexcept;
    void notify_one() noexcept;
    void notify_all() noexcept;
};
```

A `string` is one word to an object never modified, so the atomic of a string is the atomic of that word: the operations above with a string on the outside, one word in size. A load is one atomic load (with the hazard pointer of every atomic load) and the string it returns is the object as it was, whatever is stored meanwhile; a store is the store of the object the caller has already made; nothing is copied and nothing allocated beyond the strings themselves. The compare-exchanges compare identity, the object, as a compare-exchange on a word does: two strings of the same characters made apart are two objects, and the expected one must be the one loaded or stored from here, not one equal to it (`sgcl::string("a")` is never the string that is there); an exchange for a change of contents loads, decides, and exchanges against what it loaded. `atomic<string>` lives where a `string` does, on a stack or inside a managed object.

```cpp
static atomic<string> current_host = string("localhost");   // a global, read by every thread

string host = current_host.load();                 // one load: the string as it was
current_host = string("db.internal");              // a store: the old one dies with its last reader
string seen = current_host.load();
if (!current_host.compare_exchange_strong(seen, string("db2.internal"))) {
    // someone stored since: seen is what is there now
}
```

## Example

```cpp
#include "sgcl/sgcl.h"
#include <cassert>
#include <iostream>

using namespace sgcl;

// A configuration replaced at run time under readers on other threads: the
// readers load it, the writer stores a new one, and the old one is
// collected when the last reader drops it. No lock, no count.
struct Config {
    explicit Config(int version) : version(version) {}
    int version;
};

int main() {
    atomic current = make_tracked<Config>(0);   // on main's stack, where a tracked_ptr may live

    vector<thread> readers;
    for (int t : range(4)) {
        readers.emplace_back([&current] {                // by reference: main's frame outlives the threads it joins
            int last = -1;
            for (int i : range(100000)) {
                tracked_ptr config = current.load();      // held: cannot be reclaimed under this thread
                assert(config->version >= last);              // versions only go up
                last = config->version;
            }
        });
    }

    for (int version : range(1, 101)) {
        current.store(make_tracked<Config>(version));      // the old Config lives on for its readers
    }
    for (auto& r : readers) {
        r.join();
    }
    std::cout << "final version " << current.load()->version << '\n';   // 100

    collector::force_collect(true);     // optional, for the demonstration only: the collector runs its cycles by itself
    return 0;
}
```

The output:

```
final version 100
```

## See also

- [atomic_ref](atomic_ref.md): the same operations on a `tracked_ptr` that is not declared atomic
- [tracked_ptr](tracked_ptr.md), [unique_ptr](unique_ptr.md), [make_tracked](make_tracked.md)
- README: [The classes](README.md#the-classes), [The rules](README.md#the-rules), [Threads](../async/README.md#threads), [Benchmarks: Lock-free stack](../concurrent/benchmarks.md#lock-free-stack)
- `examples/lock_free_stack.cpp`, `examples/threads.cpp`
