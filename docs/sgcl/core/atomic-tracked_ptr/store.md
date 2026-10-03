[sgcl](../../README.md) › [core](../README.md) › [atomic](../atomic.md) › [tracked_ptr](README.md)

# sgcl::atomic\<tracked_ptr\<T\>\>::store

```cpp
void store(std::nullptr_t, const std::memory_order m = std::memory_order_seq_cst) noexcept;    // (1)
void store(unique_ptr<T>&& p,                                                                  // (2)
           const std::memory_order m = std::memory_order_seq_cst) noexcept;
void store(tracked_ptr<T> p,                                                                   // (3)
           const std::memory_order m = std::memory_order_seq_cst) noexcept;
```

Replaces the pointer. The old object lives on for whoever holds it; the store carries the write barrier.

1. With null.
2. With the object of `p`, which leaves the unique state on the way in and is the collector's from here on; `p` is
   null after.
3. With a copy of `p`, taken by value so that its target is held for the length of the call.

## Parameters

| Parameter | Description |
|---|---|
| `p` | the pointer stored |
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
    atomic<tracked_ptr<int>> a;
    a.store(make_tracked<int>(1));  // from a unique_ptr
    tracked_ptr old = a.load();

    tracked_ptr two = make_tracked<int>(2);
    a.store(two, std::memory_order_release);  // from a tracked_ptr
    println("{} {}", *old, *a.load());  // the old object lives on for its holder

    a.store(nullptr);
    println("{}", a.load() == nullptr);
}
```

Output:

```text
1 2
true
```

## See also

- [load, operator tracked_ptr\<T\>](load.md): reads the pointer
- [exchange](exchange.md): replaces it and returns the old one
- [sgcl::atomic\<tracked_ptr\<T\>\>](README.md)
