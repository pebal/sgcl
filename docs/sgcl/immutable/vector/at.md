[sgcl](../../README.md) › [immutable](../README.md) › [vector](README.md)

# sgcl::immutable::vector\<T\>::at

```cpp
const_reference at(size_type i) const;
```

Returns a reference to the element at `i`, with bounds checking: an `i` outside the vector throws. The element is
in the tail for the last 32 positions, and otherwise in a leaf reached through `depth()` branches.

## Parameters

| Parameter | Description |
|---|---|
| `i` | the position of the element |

## Return value

A `const` reference to the element.

## Complexity

Logarithmic in `size()`, base 32: `depth()` branches walked; constant for the last 32 elements.

## Exceptions

`out_of_range` when `i >= size()`.

## Notes

[operator[]](operator_at.md) is the same access without the check. The reference is valid while some vector
holds the leaf: the version it was read from, or any version that shares the leaf.

## Example

```cpp
#include "sgcl/immutable.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    immutable::vector<int> v = {10, 20, 30};
    println("{}", v.at(1));

    try {
        println("{}", v.at(3));
    } catch (const out_of_range& e) {
        println("out of range: {}", e.what());
    }
}
```

Output:

```text
20
out of range: sgcl::immutable::vector::at
```

## See also

- [operator[]](operator_at.md): access an element without the check
- [set](set.md): the vector with an element replaced
- [sgcl::immutable::vector\<T\>](README.md)
