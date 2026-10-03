[sgcl](../../README.md) › [core](../README.md) › [atomic](../atomic.md) › [handle](../atomic-handle.md)

# sgcl::atomic\<H\>::load, operator H

```cpp
/*(1)*/ H load(const std::memory_order m = std::memory_order_seq_cst) const noexcept;
/*(2)*/ operator H() const noexcept;
```

1. Reads the handle: one atomic load of the word, with the hazard pointer of every atomic load
   ([atomic\<tracked_ptr\<T\>\>::load](../atomic-tracked_ptr/load.md)), and a handle made of it. The handle returned
   holds the object as it was, whatever is stored meanwhile; nothing is copied.
2. `load()`: `H h = a` reads the handle.

The read after the hazard's store is `seq_cst` whatever `m` asks for; the load is never weaker than `acquire`.

## Parameters

| Parameter | Description |
|---|---|
| `m` | the memory order, as for `std::atomic::load` |

## Return value

A handle to the object the atomic held at the load.

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
    atomic<string> status = string("starting");
    string before = status.load(std::memory_order_acquire);
    status = string("running");
    string now = status;  // operator string
    println("{} -> {}", before, now);  // before still holds the old object
}
```

Output:

```text
starting -> running
```

## See also

- [store](store.md): replaces the handle
- [exchange](exchange.md): replaces it and returns the old one
- [sgcl::atomic\<H\>](../atomic-handle.md)
