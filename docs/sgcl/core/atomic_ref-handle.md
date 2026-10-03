[sgcl](../README.md) › [core](README.md) › [atomic_ref](atomic_ref.md)

# sgcl::atomic_ref\<H\>

```cpp
#include "sgcl/core/atomic_ref.h"   // or "sgcl/core.h"

namespace sgcl {
    template<req::handle H>
    class atomic_ref<H>;
}
```

`sgcl::atomic_ref<H>` is the atomic view of a handle ([req::handle](req/handle.md): a [string](string.md),
`io::file`, `net::connection`, `async::channel`, every public type that is one tracked word to the object inside)
that lives somewhere already: the operations of [atomic\<H\>](atomic-handle.md) on the handle's word, with the
handle on the outside. It serves a handle that is a member of a managed object, or the one a [rooted](rooted.md)
handle holds (`static rooted<io::file> log(...); atomic_ref a(*log);`), replaced by one thread while others read
it. The compare-exchanges compare the object, not its contents, as those of `atomic<H>` do.

## Rules

- The handle referred to lives where it may (a stack, a managed object, inside a `rooted`), and outlives every
  `atomic_ref` to it: it must not be moved or destroyed while a view of it exists. The `atomic_ref` itself is a
  plain reference and may live anywhere, like `std::atomic_ref`.
- While any `atomic_ref` to a handle exists, the handle is accessed only through `atomic_ref`s, as with
  `std::atomic_ref`.
- Copyable (the copy refers to the same handle), not assignable.
- Every operation is lock-free (`is_always_lock_free`) and may be called from any thread.

## Template parameters

| Parameter | Description |
|---|---|
| `H` | The handle: a type that satisfies [req::handle](req/handle.md). |

## Member types

| Type | Definition |
|---|---|
| `value_type` | `H` |

## Member objects

| Object | Value | Description |
|---|---|---|
| `ref` | the constructor's argument | the handle referred to, `H&`, public |
| `is_always_lock_free` | `true` on every platform the library supports | `std::atomic<void*>::is_always_lock_free`; `static constexpr bool` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](atomic_ref-handle/atomic_ref-handle.md) | constructs a view of a handle |
| [operator=](atomic_ref-handle/operator_assign.md) | stores a handle |
| [is_lock_free](atomic_ref-handle/is_lock_free.md) | checks whether the operations are lock-free |
| [store](atomic_ref-handle/store.md) | replaces the handle |
| [load, operator H](atomic_ref-handle/load.md) | reads the handle |
| [exchange](atomic_ref-handle/exchange.md) | replaces the handle and returns the old one |
| [compare_exchange_weak, compare_exchange_strong](atomic_ref-handle/compare_exchange.md) | replaces the handle when it holds the object expected |
| [wait](atomic_ref-handle/wait.md) | blocks while the handle holds the object given |
| [notify_one](atomic_ref-handle/notify_one.md) | wakes a thread blocked in `wait` |
| [notify_all](atomic_ref-handle/notify_all.md) | wakes every thread blocked in `wait` |

## Deduction guides

```cpp
template<class T>
atomic_ref(T&) -> atomic_ref<T>;
```

`atomic_ref(h)` for a handle `h` is an `atomic_ref<H>` over its word.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

static rooted<string> current_host(string("localhost"));  // a global, read by every thread

int main() {
    vector<thread> readers;
    atomic<int> seen_new = 0;
    for (int t : range(4)) {
        readers.emplace_back([&seen_new] {
            string host;
            do {
                host = atomic_ref(*current_host).load();  // one load: the string as it was
            } while (host == string("localhost"));
            ++seen_new;
        });
    }
    // the old string dies with its last reader
    atomic_ref(*current_host).store(string("db.internal"));
    for (auto& r : readers) {
        r.join();
    }
    println("{} readers saw {}", seen_new.load(), atomic_ref(*current_host).load());
}
```

Output:

```text
4 readers saw db.internal
```

## See also

- [atomic\<H\>](atomic-handle.md): the same operations on a handle declared atomic
- [atomic_ref\<tracked_ptr\<T\>\>](atomic_ref-tracked_ptr.md): the atomic view of a tracked pointer
- [req::handle](req/handle.md), [rooted](rooted.md), [string](string.md)
- [sgcl::atomic_ref\<T\>](atomic_ref.md)
