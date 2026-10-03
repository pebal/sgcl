[sgcl](../../README.md) › [core](../README.md) › [deque](../deque.md)

# sgcl::deque\<T\>::at

```cpp
/*(1)*/ reference at(size_type pos);
/*(2)*/ const_reference at(size_type pos) const;
```

Returns a reference to the element at `pos`, with bounds checking: a `pos` outside the deque throws.

## Parameters

| Parameter | Description |
|---|---|
| `pos` | the position of the element |

## Return value

A reference to the element.

## Complexity

Constant: a division by the block size and two loads.

## Exceptions

`out_of_range` when `pos >= size()`.

## Notes

[operator[]](operator_at.md) is the same access without the check. The reference stays valid across a push or a
pop at either end, as with `std::deque`, until the element is removed or an insertion or an erasure in the middle
moves the elements.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    deque d = {10, 20, 30};
    d.at(1) = 25;
    println("{}", d);

    try {
        d.at(3) = 40;
    } catch (const out_of_range& e) {
        println("out of range: {}", e.what());
    }
}
```

Output:

```text
[10, 25, 30]
out of range: sgcl::deque::at
```

## See also

- [operator[]](operator_at.md): access an element without the check
- [front](front.md), [back](back.md): access the first, the last element
- [sgcl::deque\<T\>](../deque.md)
