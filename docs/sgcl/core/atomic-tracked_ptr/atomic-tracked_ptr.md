[sgcl](../../README.md) › [core](../README.md) › [atomic](../atomic.md) › [tracked_ptr](../atomic-tracked_ptr.md)

# sgcl::atomic\<tracked_ptr\<T\>\>::atomic

```cpp
atomic() noexcept;                     // (1)
atomic(std::nullptr_t) noexcept;       // (2)
atomic(unique_ptr<T>&& p) noexcept;    // (3)
atomic(value_type p) noexcept;         // (4)
atomic(const atomic&) = delete;        // (5)
```

Constructs the atomic, as the constructors of `std::atomic` do.

1. Null.
2. Null.
3. The object of `p`, which leaves the unique state on the way in and is the collector's from here on; `p` is null
   after.
4. A copy of `p`.
5. An atomic is neither copyable nor movable.

The initialization is not an atomic operation, as with `std::atomic`.

## Parameters

| Parameter | Description |
|---|---|
| `p` | the pointer the atomic starts with |

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Node {
    int value = 0;
};

int main() {
    atomic<tracked_ptr<Node>> empty;  // null
    atomic<tracked_ptr<Node>> made(make_tracked<Node>(1));  // from a unique_ptr
    tracked_ptr node = make_tracked<Node>(2);
    atomic<tracked_ptr<Node>> shared(node);  // from a tracked_ptr
    atomic deduced = make_tracked<Node>(3);  // atomic<tracked_ptr<Node>>

    println("{} {} {} {}", empty.load() == nullptr, made.load()->value, shared.load()->value,
            deduced.load()->value);
}
```

Output:

```text
true 1 2 3
```

## See also

- [operator=](operator_assign.md), [store](store.md): replace the pointer
- [sgcl::atomic\<tracked_ptr\<T\>\>](../atomic-tracked_ptr.md)
