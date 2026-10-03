[sgcl](../../README.md) › [core](../README.md) › [atomic_ref](../atomic_ref.md) › [handle](../atomic_ref-handle.md)

# sgcl::atomic_ref\<H\>::store

```cpp
void store(const H& h, const std::memory_order m = std::memory_order_seq_cst) noexcept;
```

Replaces the handle viewed: it holds the object `h` holds, and the old object lives on for whoever holds it. The
store is the store of the word, with the write barrier; nothing is copied.

## Parameters

| Parameter | Description |
|---|---|
| `h` | the handle stored |
| `m` | the memory order, as for `std::atomic_ref::store` |

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

struct Board {
    string motd = "welcome";
};

int main() {
    tracked_ptr board = make_tracked<Board>();
    string old = atomic_ref(board->motd).load();
    atomic_ref(board->motd).store(string("maintenance at 22:00"), std::memory_order_release);
    println("{} | {}", old, atomic_ref(board->motd).load());
}
```

Output:

```text
welcome | maintenance at 22:00
```

## See also

- [load, operator H](load.md): reads the handle
- [exchange](exchange.md): replaces it and returns the old one
- [sgcl::atomic_ref\<H\>](../atomic_ref-handle.md)
