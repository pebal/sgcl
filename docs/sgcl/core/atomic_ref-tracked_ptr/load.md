[sgcl](../../README.md) › [core](../README.md) › [atomic_ref](../atomic_ref.md) › [tracked_ptr](../atomic_ref-tracked_ptr.md)

# sgcl::atomic_ref\<tracked_ptr\<T\>\>::load, operator tracked_ptr\<T\>

```cpp
tracked_ptr<T> load(const std::memory_order m = std::memory_order_seq_cst) const noexcept;    // (1)
operator tracked_ptr<T>() const noexcept;                                                     // (2)
```

1. Reads the pointer viewed, as a `tracked_ptr` that holds the object: the word read twice around a hazard pointer
   published on the calling thread's record, so that the collector cannot reclaim the object between the read and
   the hold, as [atomic\<tracked_ptr\<T\>\>::load](../atomic-tracked_ptr/load.md) reads it.
2. `load()`: `tracked_ptr<T> p = a` reads the pointer.

The read after the hazard's store is `seq_cst` whatever `m` asks for; the load is never weaker than `acquire`.

## Parameters

| Parameter | Description |
|---|---|
| `m` | the memory order, as for `std::atomic_ref::load` |

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
    tracked_ptr word = make_tracked<int>(1);
    tracked_ptr p = atomic_ref(word).load(std::memory_order_acquire);  // the 1, held
    atomic_ref(word).store(make_tracked<int>(2));
    tracked_ptr<int> q = atomic_ref(word);  // operator tracked_ptr<int>
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
- [sgcl::atomic_ref\<tracked_ptr\<T\>\>](../atomic_ref-tracked_ptr.md)
