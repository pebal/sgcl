[sgcl](../../README.md) › [core](../README.md) › [atomic](../atomic.md) › [tracked_ptr](../atomic-tracked_ptr.md)

# sgcl::atomic\<tracked_ptr\<T\>\>::load, operator tracked_ptr\<T\>

```cpp
tracked_ptr<T> load(const std::memory_order m = std::memory_order_seq_cst) const noexcept;    // (1)
operator tracked_ptr<T>() const noexcept;                                                     // (2)
```

1. Reads the pointer, as a `tracked_ptr` that holds the object. The word is read, a hazard pointer to it published
   on the calling thread's record, and the word read again until the two reads agree; the `tracked_ptr` is then
   constructed and the hazard cleared. The collector reads the hazards before it reclaims anything, so the object
   cannot be freed between the read and the hold.
2. `load()`: `tracked_ptr<T> p = a` reads the pointer.

The read after the hazard's store is `seq_cst` whatever `m` asks for: the store of the hazard must be visible
before the word is read again, which an acquire load does not promise. `m` is taken for the interface of
`std::atomic`; the load is never weaker than `acquire`.

## Parameters

| Parameter | Description |
|---|---|
| `m` | the memory order, as for `std::atomic::load` |

## Return value

The pointer, held: the object lives at least as long as the returned `tracked_ptr`, whatever is stored meanwhile.

## Complexity

Constant: two reads of the word around a store of the hazard pointer, more while other threads change the word.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    atomic<tracked_ptr<int>> a(make_tracked<int>(1));
    tracked_ptr p = a.load(std::memory_order_acquire);  // tracked_ptr<int>: the 1, held
    a = make_tracked<int>(2);  // p still holds the 1
    tracked_ptr<int> q = a;  // operator tracked_ptr<int>
    println("{} {}", *p, *q);
}
```

Output:

```text
1 2
```

## See also

- [store](store.md): replaces the pointer
- [exchange](exchange.md): replaces it and returns the old one
- [sgcl::atomic\<tracked_ptr\<T\>\>](../atomic-tracked_ptr.md)
