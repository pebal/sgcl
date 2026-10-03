[sgcl](../README.md) › [core](README.md)

# sgcl::atomic_ref\<T\>

```cpp
#include "sgcl/core/atomic_ref.h"   // or "sgcl/core.h"

namespace sgcl {
    template<class T>
    class atomic_ref;   // std::atomic_ref<T>, for every T but the two below

    template<class T>
    class atomic_ref<tracked_ptr<T>>;

    template<req::handle H>
    class atomic_ref<H>;
}
```

`sgcl::atomic_ref<T>` is `std::atomic_ref<T>` for every `T` but two, as [atomic\<T\>](atomic.md) is
`std::atomic<T>`: it derives from `std::atomic_ref<T>` and takes over its constructors and assignments, so that a
program names one `atomic_ref` for its counters and its pointers alike (deduced: `atomic_ref a(counter)`). The two
that differ are the views of the library's own over one word:

- a [tracked_ptr](tracked_ptr.md) that lives somewhere already, a member of a node, an element of a vector, the
  word a [root_ptr](root_ptr.md) holds its object by: [atomic_ref\<tracked_ptr\<T\>\>](atomic_ref-tracked_ptr.md),
  with the operations of [atomic\<tracked_ptr\<T\>\>](atomic-tracked_ptr.md);
- a handle ([req::handle](req/handle.md): a [string](string.md), `io::file`, `net::connection`, `async::channel`):
  [atomic_ref\<H\>](atomic_ref-handle.md), with the operations of [atomic\<H\>](atomic-handle.md).

A global holding an object that other threads share and that is replaced at run time is a `root_ptr` under an
`atomic_ref`, or a [rooted](rooted.md) handle under one: `atomic_ref a(root)`, `atomic_ref a(*rooted_handle)`.

## Rules

- For a plain `T`, `atomic_ref<T>` is `std::atomic_ref<T>` in every respect: the object it refers to is accessed
  only through `atomic_ref`s while one exists, and outlives them all.
- The `atomic_ref` itself is a plain reference and may live anywhere, like `std::atomic_ref`; what it refers to
  lives where its type may.

## Template parameters

| Parameter | Description |
|---|---|
| `T` | The type of the object referred to: what `std::atomic_ref<T>` takes, a `tracked_ptr<T>` or a handle. |

## Member functions

Those of [std::atomic_ref\<T\>](https://en.cppreference.com/w/cpp/atomic/atomic_ref), with its constructors and
its assignments. The specializations have their own:
[atomic_ref\<tracked_ptr\<T\>\>](atomic_ref-tracked_ptr.md#member-functions),
[atomic_ref\<H\>](atomic_ref-handle.md#member-functions).

## Deduction guides

```cpp
template<class T>
atomic_ref(T&) -> atomic_ref<T>;
template <class T>
atomic_ref(tracked_ptr<T>) -> atomic_ref<tracked_ptr<T>>;
template <class T>
atomic_ref(root_ptr<T>) -> atomic_ref<tracked_ptr<T>>;
```

`sgcl::atomic_ref(p)` for a `tracked_ptr<T> p` is an `atomic_ref<tracked_ptr<T>>`; for a `root_ptr<T> r` the same,
over the word `r` holds its object by; for a handle `h` (a `string`, an `io::file`) an `atomic_ref<H>` over its
word; for an `int` an `atomic_ref<int>`.

## Specializations

| Specialization | Description |
|---|---|
| [atomic_ref\<tracked_ptr\<T\>\>](atomic_ref-tracked_ptr.md) | the atomic view of a `tracked_ptr`: lock-free, the loaded object held, no ABA |
| [atomic_ref\<H\>](atomic_ref-handle.md) | the atomic view of a handle (`req::handle`): the handle's word, compared by identity |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Stats {
    int hits = 0;
    tracked_ptr<string> last;
};

int main() {
    tracked_ptr stats = make_tracked<Stats>();
    vector<thread> workers;
    for (int t : range(4)) {
        workers.emplace_back([stats] {
            for (int i : range(1000)) {
                atomic_ref(stats->hits).fetch_add(1);  // std::atomic_ref<int>
            }
            atomic_ref(stats->last).store(make_tracked<string>("worker"));  // a tracked_ptr
        });
    }
    for (auto& w : workers) {
        w.join();
    }
    println("{} {}", atomic_ref(stats->hits).load(), *atomic_ref(stats->last).load());
}
```

Output:

```text
4000 worker
```

## See also

- [atomic](atomic.md): the same operations on a value declared atomic
- [tracked_ptr](tracked_ptr.md), [root_ptr](root_ptr.md), [rooted](rooted.md), [req::handle](req/handle.md)
- [README: Pointers](README.md#pointers), [README: The rules](README.md#the-rules),
  [Threads](../async/README.md#threads)
