[sgcl](../../README.md) › [core](../README.md) › [atomic_ref](../atomic_ref.md) › [handle](../atomic_ref-handle.md)

# sgcl::atomic_ref\<H\>::operator=

```cpp
/*(1)*/ H operator=(const H& h) noexcept;
/*(2)*/ atomic_ref& operator=(const atomic_ref&) = delete;
```

1. `store(h)`, with `std::memory_order_seq_cst`: the handle viewed holds the object `h` holds.
2. A view is not assignable: it refers to one handle for its life.

## Parameters

| Parameter | Description |
|---|---|
| `h` | the handle stored |

## Return value

`h`.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string greeting;
    atomic_ref a(greeting);
    a = string("hello");  // a store
    string text = a;  // a load
    println("{}", text);
}
```

Output:

```text
hello
```

## See also

- [store](store.md): the same with a memory order
- [load, operator H](load.md): the read
- [sgcl::atomic_ref\<H\>](../atomic_ref-handle.md)
