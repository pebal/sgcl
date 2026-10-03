[sgcl](../../README.md) › [core](../README.md) › [atomic](../atomic.md) › [tracked_ptr](../atomic-tracked_ptr.md)

# sgcl::atomic\<tracked_ptr\<T\>\>::operator=

```cpp
std::nullptr_t operator=(std::nullptr_t) noexcept;    // (1)
void operator=(unique_ptr<T>&& p) noexcept;           // (2)
value_type operator=(value_type p) noexcept;          // (3)
atomic& operator=(const atomic&) = delete;            // (4)
```

Stores a pointer, as `store` with `std::memory_order_seq_cst`.

1. `store(nullptr)`.
2. `store(std::move(p))`: the object of `p` leaves the unique state and is the collector's from here on.
3. `store(p)`.
4. An atomic is not assignable from another.

Unlike the `operator=` of `std::atomic`, the store of a `unique_ptr` returns nothing: the object it held is no
longer its to return.

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
    atomic<tracked_ptr<int>> a;
    a = make_tracked<int>(1);  // a store
    tracked_ptr<int> p = a;  // a load, seq_cst
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
- [sgcl::atomic\<tracked_ptr\<T\>\>](../atomic-tracked_ptr.md)
