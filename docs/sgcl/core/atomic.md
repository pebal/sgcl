[sgcl](../README.md) › [core](README.md)

# sgcl::atomic\<T\>

```cpp
#include "sgcl/core/atomic.h"   // or "sgcl/core.h"

namespace sgcl {
    template<class T>
    class atomic;   // std::atomic<T>, for every T but the two below

    template<class T>
    class atomic<tracked_ptr<T>>;

    template<req::handle H>
    class atomic<H>;
}
```

`sgcl::atomic<T>` is `std::atomic<T>` for every `T` but two: it derives from `std::atomic<T>` and takes over its
constructors and assignments, so that a program names one atomic for its flags, its counters and its pointers
alike. The two that differ have atomics of the library's own, over the one word each of them is:

- a [tracked_ptr](tracked_ptr/README.md): [atomic\<tracked_ptr\<T\>\>](atomic-tracked_ptr/README.md), a word that several
  threads read and write without a lock, safe against reuse and free of ABA;
- a handle, a public type of the library that is one tracked word to the object inside it ([req::handle](req/handle.md):
  a [string](string/README.md), `io::file`, `net::connection`, `async::channel`): [atomic\<H\>](atomic-handle/README.md), the
  same word with the handle on the outside.

A `tracked_ptr` written by one thread and read by another needs `atomic`, [atomic_ref](atomic_ref.md) or the
program's own synchronization ([The rules](README.md#the-rules), 6). A value shared whole between threads, an
object rather than a word, is a [copy_on_write](../concurrent/copy_on_write/README.md).

## Rules

- For a plain `T` (an `int`, a `bool`, a raw pointer, an enumeration), `atomic<T>` is `std::atomic<T>` in every
  respect: where it may live, what it costs, what it guarantees.
- The specializations live where a `tracked_ptr` may: on a stack or inside a managed object
  ([The rules](README.md#the-rules), 1).

## Template parameters

| Parameter | Description |
|---|---|
| `T` | The type of the value: what `std::atomic<T>` takes, a `tracked_ptr<T>` or a handle. |

## Member functions

Those of [std::atomic\<T\>](https://en.cppreference.com/w/cpp/atomic/atomic), with its constructors and its
assignments. The specializations have their own: [atomic\<tracked_ptr\<T\>\>](atomic-tracked_ptr/README.md#member-functions),
[atomic\<H\>](atomic-handle/README.md#member-functions).

## Deduction guides

```cpp
template<class T>
atomic(unique_ptr<T>&&) -> atomic<tracked_ptr<T>>;
template<class T>
atomic(tracked_ptr<T>) -> atomic<tracked_ptr<T>>;
template<class T>
atomic(T) -> atomic<T>;
```

The atomic of what the initializer holds: `atomic current = make_tracked<Config>()` is an
`atomic<tracked_ptr<Config>>`, `atomic count = 0` an `atomic<int>`, `atomic host = string("db")` an `atomic<string>`.

## Specializations

| Specialization | Description |
|---|---|
| [atomic\<tracked_ptr\<T\>\>](atomic-tracked_ptr/README.md) | the atomic of a tracked pointer: lock-free, one word, the loaded object held, no ABA |
| [atomic\<H\>](atomic-handle/README.md) | the atomic of a handle (`req::handle`): the handle's word, compared by identity |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Config {
    int version;
};

int main() {
    atomic<int> hits = 0;  // std::atomic<int>
    atomic<bool> ready = false;
    atomic current = make_tracked<Config>(1);  // atomic<tracked_ptr<Config>>
    atomic host = string("localhost");  // atomic<string>

    vector<thread> workers;
    for (int t : range(4)) {
        workers.emplace_back([&] {
            for (int i : range(1000)) {
                ++hits;
            }
        });
    }
    for (auto& w : workers) {
        w.join();
    }
    current = make_tracked<Config>(2);
    host = string("db.internal");
    ready = true;
    println("{} {} {} {}", hits.load(), ready.load(), current.load()->version, host.load());
}
```

Output:

```text
4000 true 2 db.internal
```

## See also

- [atomic_ref](atomic_ref.md): the same operations over a value that is not declared atomic
- [tracked_ptr](tracked_ptr/README.md), [string](string/README.md), [req::handle](req/handle.md)
- [copy_on_write](../concurrent/copy_on_write/README.md): a value shared whole between threads
- [README: Pointers](README.md#pointers), [README: The rules](README.md#the-rules),
  [Threads](../async/README.md#threads)
