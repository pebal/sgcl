[sgcl](../../README.md) › [core](../README.md) › [atomic_ref](../atomic_ref.md) › [tracked_ptr](README.md)

# sgcl::atomic_ref\<tracked_ptr\<T\>\>::store

```cpp
void store(std::nullptr_t, const std::memory_order m = std::memory_order_seq_cst) noexcept;    // (1)
void store(unique_ptr<T>&& p,                                                                  // (2)
           const std::memory_order m = std::memory_order_seq_cst) noexcept;
void store(tracked_ptr<T> p,                                                                   // (3)
           const std::memory_order m = std::memory_order_seq_cst) noexcept;
```

Replaces the pointer viewed. The old object lives on for whoever holds it; the store carries the write barrier.

1. With null.
2. With the object of `p`, which leaves the unique state on the way in and is the collector's from here on; `p` is
   null after.
3. With a copy of `p`, taken by value so that its target is held for the length of the call.

## Parameters

| Parameter | Description |
|---|---|
| `p` | the pointer stored |
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

struct Slot {
    tracked_ptr<int> value;
};

int main() {
    tracked_ptr slot = make_tracked<Slot>();
    atomic_ref(slot->value).store(make_tracked<int>(1));  // from a unique_ptr
    tracked_ptr two = make_tracked<int>(2);
    atomic_ref(slot->value).store(two, std::memory_order_release);  // from a tracked_ptr
    println("{}", *slot->value);
    atomic_ref(slot->value).store(nullptr);
    println("{}", slot->value == nullptr);
}
```

Output:

```text
2
true
```

## See also

- [load, operator tracked_ptr\<T\>](load.md): reads the pointer
- [exchange](exchange.md): replaces it and returns the old one
- [sgcl::atomic_ref\<tracked_ptr\<T\>\>](README.md)
