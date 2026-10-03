[sgcl](../../README.md) › [core](../README.md) › [atomic_ref](../atomic_ref.md) › [tracked_ptr](../atomic_ref-tracked_ptr.md)

# sgcl::atomic_ref\<tracked_ptr\<T\>\>::operator=

```cpp
std::nullptr_t operator=(std::nullptr_t) noexcept;    // (1)
void operator=(unique_ptr<T>&& p) noexcept;           // (2)
value_type operator=(value_type p) noexcept;          // (3)
atomic_ref& operator=(const atomic_ref&) = delete;    // (4)
```

Stores a pointer into the word viewed, as `store` with `std::memory_order_seq_cst`.

1. `store(nullptr)`.
2. `store(std::move(p))`: the object of `p` leaves the unique state and is the collector's from here on.
3. `store(p)`.
4. A view is not assignable: it refers to one pointer for its life.

## Parameters

| Parameter | Description |
|---|---|
| `p` | the pointer stored |

## Return value

- (1) `nullptr`.
- (2) None.
- (3) `p`.

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
    tracked_ptr<int> word;
    atomic_ref a(word);
    a = make_tracked<int>(1);  // a store
    tracked_ptr<int> p = a;  // a load
    a = nullptr;
    println("{} {}", *p, a.load() == nullptr);
}
```

Output:

```text
1 true
```

## See also

- [store](store.md): the same with a memory order
- [load, operator tracked_ptr\<T\>](load.md): the read
- [sgcl::atomic_ref\<tracked_ptr\<T\>\>](../atomic_ref-tracked_ptr.md)
