[sgcl](../../README.md) › [core](../README.md) › [mixin](README.md)

# sgcl::mixin::immutable\<Derived\>

```cpp
#include "sgcl/core/mixin/immutable.h"   // or "sgcl/core.h"

namespace sgcl::mixin {
    template<class Derived>
    class immutable;
}
```

`sgcl::mixin::immutable<Derived>` is a declaration without methods: that the container is a value that never
changes. No method writes an element in place; every change — `set`, `push_back`, `insert`, `erase` — is a
`const` method that returns a new container sharing the old one's structure, and a copy is one word.
`req::immutable<R>` is "R carries `mixin::immutable`" ([the mixins](README.md)): a function that takes
`const req::immutable auto&` can keep what it was given, hand it to another thread or compare it with a later
version, without a copy and without a lock, because nothing it holds will change under it. The four immutable
containers carry it: [immutable::vector](../../immutable/vector.md), [immutable::list](../../immutable/list.md),
[immutable::map](../../immutable/map.md), [immutable::set](../../immutable/set.md).

Not the same as not being written: `slice<const T>` and `sorted_set` carry no [mixin::sequence](sequence.md)
either, but the object under a `slice<const T>` may change behind it and a `sorted_set` has `insert`. What they
do not say, `mixin::immutable` says: the value is final.

## Rules

- A class that carries it keeps the promise itself: no non-`const` method that changes an element, every change a
  new object. The mixin declares, it does not check.
- Thread safety follows from the promise: a value that never changes is read from any thread without
  synchronization; the handles (the `tracked_ptr` to the root) are exchanged with the program's own.

## Template parameters

| Parameter | Description |
|---|---|
| `Derived` | The container that carries the mixin and names itself as the argument (`class vector : public mixin::immutable<vector<T>>`). |

## Member functions

| Function | Description |
|---|---|
| `(constructor)`, `(destructor)` | protected: the mixin exists only as a base; it has no other member |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/immutable.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    immutable::vector<int> first = immutable::vector<int>().push_back(1);
    immutable::vector<int> second = first.push_back(2);
    println("{} {}", first, second);

    println("{}", req::immutable<immutable::vector<int>>);
    println("{}", req::immutable<immutable::map<int, int>>);
    println("{} {}", req::immutable<vector<int>>, req::immutable<slice<const int>>);
    println("{}", req::immutable<sorted_set<int>>);
}
```

Output:

```text
[1] [1, 2]
true
true
false false
false
```

A snapshot handed to a task: the task reads the version it was given while the main thread moves on to newer
ones, with no copy and no lock, which the parameter's requirement is the promise of.

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/immutable.h"
#include "sgcl/io.h"

using namespace sgcl;

template<req::immutable R>
async::task<size_t> count_even(R snapshot) {
    co_await async::sleep(std::chrono::milliseconds(1));
    co_return snapshot.count_of([](int x) { return x % 2 == 0; });
}

int main() {
    immutable::vector<int> v;
    for (int i : range(8)) {
        v = v.push_back(i);
    }
    auto evens = async::spawn(count_even(v));  // running on a worker: the task holds version v
    for (int i : range(8, 16)) {
        v = v.push_back(i * 2);  // newer versions: the task's is untouched
    }
    size_t n = evens.wait();
    println("{} even of the first eight, {} in the latest", n, v.size());
}
```

Output:

```text
4 even of the first eight, 16 in the latest
```

## See also

- [req::immutable](../req/immutable.md): a range that never changes: what a function asks for to keep one without
  a copy
- [the mixins and the requirements](README.md); [immutable](../../immutable/README.md): the containers that carry
  it
- `tests/core/mixin.cpp`
