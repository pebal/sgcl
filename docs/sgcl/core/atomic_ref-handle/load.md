[sgcl](../../README.md) › [core](../README.md) › [atomic_ref](../atomic_ref.md) › [handle](../atomic_ref-handle.md)

# sgcl::atomic_ref\<H\>::load, operator H

```cpp
H load(const std::memory_order m = std::memory_order_seq_cst) const noexcept;    // (1)
operator H() const noexcept;                                                     // (2)
```

1. Reads the handle viewed: one atomic load of its word, with the hazard pointer of every atomic load, and a handle
   made of it, as [atomic\<H\>::load](../atomic-handle/load.md) reads it. The handle returned holds the object as it
   was, whatever is stored meanwhile; nothing is copied.
2. `load()`: `H h = a` reads the handle.

The read after the hazard's store is `seq_cst` whatever `m` asks for; the load is never weaker than `acquire`.

## Parameters

| Parameter | Description |
|---|---|
| `m` | the memory order, as for `std::atomic_ref::load` |

## Return value

A handle to the object the handle viewed held at the load.

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
    string status = "starting";
    string before = atomic_ref(status).load(std::memory_order_acquire);
    atomic_ref(status).store(string("running"));
    string now = atomic_ref(status);  // operator string
    println("{} -> {}", before, now);
}
```

Output:

```text
starting -> running
```

## See also

- [store](store.md): replaces the handle
- [exchange](exchange.md): replaces it and returns the old one
- [sgcl::atomic_ref\<H\>](../atomic_ref-handle.md)
