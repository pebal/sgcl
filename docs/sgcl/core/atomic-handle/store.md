[sgcl](../../README.md) › [core](../README.md) › [atomic](../atomic.md) › [handle](README.md)

# sgcl::atomic\<H\>::store

```cpp
void store(const H& h, const std::memory_order m = std::memory_order_seq_cst) noexcept;
```

Replaces the handle: the atomic holds the object `h` holds, and the old object lives on for whoever holds it. The
store is the store of the word, with the write barrier; nothing is copied.

## Parameters

| Parameter | Description |
|---|---|
| `h` | the handle stored |
| `m` | the memory order, as for `std::atomic::store` |

## Return value

None.

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
    atomic<string> motd = string("welcome");
    string old = motd.load();
    motd.store(string("maintenance at 22:00"), std::memory_order_release);
    println("{} | {}", old, motd.load());
}
```

Output:

```text
welcome | maintenance at 22:00
```

## See also

- [load, operator H](load.md): reads the handle
- [exchange](exchange.md): replaces it and returns the old one
- [sgcl::atomic\<H\>](README.md)
