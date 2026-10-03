[sgcl](../../README.md) › [core](../README.md) › [atomic](../atomic.md) › [handle](README.md)

# sgcl::atomic\<H\>::operator=

```cpp
H operator=(const H& h) noexcept;             // (1)
atomic& operator=(const atomic&) = delete;    // (2)
```

1. `store(h)`, with `std::memory_order_seq_cst`: the atomic holds the object `h` holds.
2. An atomic is not assignable from another.

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
    atomic<string> greeting;
    greeting = string("hello");  // a store
    string text = greeting;  // a load, seq_cst
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
- [sgcl::atomic\<H\>](README.md)
