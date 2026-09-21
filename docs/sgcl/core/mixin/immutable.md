# sgcl::mixin::immutable

```cpp
#include "sgcl/core/mixin/immutable.h"   // or "sgcl/core/mixin/mixin.h", "sgcl/sgcl.h"

namespace sgcl::mixin {
    template<class Derived>
    class immutable;
}
```

`mixin::immutable<Derived>` is a declaration without methods: that the container is a value that never changes. No method writes an element in place; every change — `set`, `push_back`, `insert`, `erase` — is a `const` method that returns a new container sharing the old one's structure, and a copy is one word. `req::immutable<R>` is "R carries `mixin::immutable`" ([the mixins](README.md)): a function that takes `const req::immutable auto&` can keep what it was given, hand it to another thread or compare it with a later version, without a copy and without a lock, because nothing it holds will change under it. The four immutable containers carry it: [im::vector](../../containers/im/vector.md), [im::list](../../containers/im/list.md), [im::map](../../containers/im/map.md), [im::set](../../containers/im/set.md).

Not the same as not being written: `slice<const T>` and `sorted_set` carry no [mixin::sequence](sequence.md) either, but the object under a `slice<const T>` may change behind it and a `sorted_set` has `insert`. What they do not say, `mixin::immutable` says: the value is final.

## Rules

- A class that carries it keeps the promise itself: no non-`const` method that changes an element, every change a new object. The mixin declares, it does not check.
- Thread safety follows from the promise: a value that never changes is read from any thread without synchronization; the handles (the `tracked_ptr` to the root) are exchanged with the program's own.

## Members

None: a declaration.

```cpp
static_assert(req::immutable<im::vector<int>> && req::immutable<im::map<int, int>>);
static_assert(!req::immutable<vector<int>> && !req::immutable<slice<const int>> && !req::immutable<sorted_set<int>>);
```

## Example

```cpp
#include "sgcl/sgcl.h"
#include <iostream>

using namespace sgcl;

// A snapshot handed to a task: the task reads what it was given while the
// main thread moves on to newer versions, no copy and no lock, which the
// parameter's requirement is the promise of.
template<req::immutable R>
task<size_t> count_even(R snapshot) {
    co_await sgcl::sleep(std::chrono::milliseconds(1));
    co_return snapshot.count_of([](int x) { return x % 2 == 0; });
}

int main() {
    im::vector<int> v;
    for (int i : range(8)) {
        v = v.push_back(i);
    }
    auto evens = spawn(count_even(v));               // running on a worker: the task holds version v
    for (int i : range(8, 16)) {
        v = v.push_back(i * 2);                     // newer versions: the task's is untouched
    }
    size_t n = evens.join();
    std::cout << n << " even of the first eight, " << v.size() << " in the latest\n";
    return n == 4 && v.size() == 16 ? 0 : 1;
}
```

The output:

```
4 even of the first eight, 16 in the latest
```

## See also

- [the mixins and the requirements](README.md); [im](../../containers/im/README.md): the containers that carry it
- `tests/core/mixin.cpp`
