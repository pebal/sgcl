[sgcl](../../README.md) › [core](../README.md) › [atomic_ref](../atomic_ref.md) › [handle](../atomic_ref-handle.md)

# sgcl::atomic_ref\<H\>::atomic_ref

```cpp
explicit atomic_ref(H& h) noexcept;          // (1)
atomic_ref(const atomic_ref& a) noexcept;    // (2)
```

1. A view of `h`: the operations act on its word.
2. A copy of `a`: a view of the same handle.

`h` must outlive the view, and must not be moved while a view of it exists.

## Parameters

| Parameter | Description |
|---|---|
| `h` | the handle viewed |
| `a` | the view copied |

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Server {
    string name = "alpha";
};

int main() {
    tracked_ptr server = make_tracked<Server>();
    atomic_ref name(server->name);  // atomic_ref<string>, deduced
    atomic_ref same = name;  // the same handle
    name.store(string("beta"));
    println("{} {}", same.load(), server->name);
}
```

Output:

```text
beta beta
```

## See also

- [load, operator H](load.md), [store](store.md): the operations on the word
- [sgcl::atomic_ref\<H\>](../atomic_ref-handle.md)
