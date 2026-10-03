[sgcl](../../README.md) › [core](../README.md) › [weak_ptr](README.md)

# sgcl::weak_ptr\<T\>::lock

```cpp
pointer_type lock() const noexcept;
```

Returns the object as a `tracked_ptr`, held from then on, or null when the cell has been cleared or there is none.
Never a dangling pointer, never a destroyed object.

## Parameters

None.

## Return value

A `tracked_ptr<T>` to the object while it is reachable; null once a cycle has found it unreachable, or for an empty
`weak_ptr`.

## Complexity

Constant.

## Exceptions

None.

## Notes

The read is the cell twice around a hazard pointer, published before the second read: the collector clears a cell
before it reads the hazards, so a `lock()` that races with the clearing either sees the null or is seen, and its
object is marked and survives the cycle. Between the object becoming unreachable and the cycle that notices,
`lock()` still returns it.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Item {
    int value = 1;
};

int main() {
    tracked_ptr item = make_tracked<Item>();
    weak_ptr cached = item;
    if (tracked_ptr p = cached.lock()) {  // the object, held by p
        p->value = 2;
    }
    println("{} {}", item->value, weak_ptr<Item>().lock() == nullptr);
}
```

Output:

```text
2 true
```

## See also

- [expired](expired.md): checks whether the cell has been cleared
- [sgcl::weak_ptr\<T\>](README.md)
