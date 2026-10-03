[sgcl](../../README.md) › [immutable](../README.md) › [vector](../vector.md)

# sgcl::immutable::vector\<T\>::operator[]

```cpp
const_reference operator[](size_type i) const noexcept;
```

Returns a reference to the element at `i`, without bounds checking. The element is in the tail for the last 32
positions, and otherwise in a leaf reached through `depth()` branches, five bits of `i` per level.

## Parameters

| Parameter | Description |
|---|---|
| `i` | the position of the element, below `size()` |

## Return value

A `const` reference to the element.

## Complexity

Logarithmic in `size()`, base 32: `depth()` branches walked; constant for the last 32 elements.

## Exceptions

None. An `i` at or above `size()` is undefined; debug builds assert.

## Notes

1.6 ns for a random position of a hundred thousand `int`s. A walk of every element goes faster through an
iterator, which walks the trie once per leaf ([begin](begin.md)). [at](at.md) is the same access with the check.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/immutable.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    immutable::vector<int> v;
    for (int i : range(1000)) {
        v = v.push_back(i * i);
    }
    println("{} {} {}", v[0], v[500], v[999]);
}
```

Output:

```text
0 250000 998001
```

## See also

- [at](at.md): access an element with bounds checking
- [front](front.md), [back](back.md): the first and the last element
- [sgcl::immutable::vector\<T\>](../vector.md)
