[sgcl](../README.md) › [core](README.md) › [atomic](atomic.md)

# sgcl::atomic\<H\>

```cpp
#include "sgcl/core/atomic.h"   // or "sgcl/core.h"

namespace sgcl {
    template<req::handle H>
    class atomic<H>;
}
```

`sgcl::atomic<H>` is the atomic of a handle: a public type that is one tracked word to the object inside it, whose
copies share that object ([req::handle](req/handle.md)): a [string](string.md), `io::file`, `io::buffer`,
`io::buffered_reader`, `io::process`, `net::connection`, `async::channel<T>`, `async::mutex`. The atomic of a handle
is the atomic of that word: the operations of [atomic\<tracked_ptr\<T\>\>](atomic-tracked_ptr.md) with the handle on
the outside, one word in size, one specialization for every handle.

A load is one atomic load, with the hazard pointer of every atomic load, and the handle it returns is the object as
it was, whatever is stored meanwhile; a store is the store of the object the caller's handle holds; nothing is
copied and nothing allocated beyond the objects themselves. The compare-exchanges compare identity, the object, as a
compare-exchange on a word does: two strings of the same characters made apart are two objects, and the expected
one must be the one loaded or stored from here, not one equal to it (`sgcl::string("a")` is never the string that is
there). An exchange for a change of contents loads, decides, and exchanges against what it loaded.

A type of the program takes part the way the library's handles do, by satisfying `req::handle`;
`tests/core/atomic_handle.cpp` shows one.

## Rules

- `atomic<H>` lives where the handle does: on a stack or inside a managed object
  ([The rules](README.md#the-rules), 1). A global handle replaced at run time is a [rooted](rooted.md) handle under
  an [atomic_ref](atomic_ref-handle.md).
- It is neither copyable nor movable, like `std::atomic`.
- Every operation is lock-free (`is_always_lock_free`) and may be called from any thread.
- The object is never written through the atomic: it loads, stores and compares the word that holds the object.

## Template parameters

| Parameter | Description |
|---|---|
| `H` | The handle: a type that satisfies [req::handle](req/handle.md). |

## Member types

| Type | Definition |
|---|---|
| `value_type` | `H` |

## Member objects

| Constant | Value | Description |
|---|---|---|
| `is_always_lock_free` | `true` on every platform the library supports | `std::atomic<void*>::is_always_lock_free`: the word is a `std::atomic` of a pointer; `static constexpr bool` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](atomic-handle/atomic-handle.md) | constructs the atomic: the handle's default, or a handle |
| `(destructor)` | destroys the atomic; the object it held is left to the collector |
| [operator=](atomic-handle/operator_assign.md) | stores a handle |
| [is_lock_free](atomic-handle/is_lock_free.md) | checks whether the operations are lock-free |
| [store](atomic-handle/store.md) | replaces the handle |
| [load, operator H](atomic-handle/load.md) | reads the handle |
| [exchange](atomic-handle/exchange.md) | replaces the handle and returns the old one |
| [compare_exchange_weak, compare_exchange_strong](atomic-handle/compare_exchange.md) | replaces the handle when it holds the object expected |
| [wait](atomic-handle/wait.md) | blocks while the handle holds the object given |
| [notify_one](atomic-handle/notify_one.md) | wakes a thread blocked in `wait` |
| [notify_all](atomic-handle/notify_all.md) | wakes every thread blocked in `wait` |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Settings {
    atomic<string> host = string("localhost");  // inside a managed object
};

int main() {
    tracked_ptr settings = make_tracked<Settings>();

    string host = settings->host.load();  // one load: the string as it was
    settings->host = string("db.internal");  // a store: the old one dies with its last reader
    println("{} {}", host, settings->host.load());

    string seen = settings->host.load();
    string equal("db.internal");  // the same characters, another object
    println("{}", settings->host.compare_exchange_strong(equal, string("db2.internal")));
    println("{}", settings->host.compare_exchange_strong(seen, string("db2.internal")));
    println("{}", settings->host.load());
}
```

Output:

```text
localhost db.internal
false
true
db2.internal
```

## See also

- [atomic_ref\<H\>](atomic_ref-handle.md): the same operations on a handle that is not declared atomic
- [atomic\<tracked_ptr\<T\>\>](atomic-tracked_ptr.md): the atomic of a tracked pointer
- [req::handle](req/handle.md), [string](string.md), [rooted](rooted.md)
- [sgcl::atomic\<T\>](atomic.md)
