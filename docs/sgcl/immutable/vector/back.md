[sgcl](../../README.md) › [immutable](../README.md) › [vector](README.md)

# sgcl::immutable::vector\<T\>::back

```cpp
const_reference back() const noexcept;
```

Returns a reference to the last element, which is always in the tail: no branch is walked.

## Parameters

None.

## Return value

A `const` reference to the last element.

## Complexity

Constant.

## Exceptions

None. `back` on an empty vector is undefined; debug builds assert.

## Example

```cpp
#include "sgcl/immutable.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    immutable::vector<int> v = {1, 2, 3};
    auto longer = v.push_back(4);
    println("{} {} {}", v.back(), longer.back(), longer.pop_back().back());
}
```

Output:

```text
3 4 3
```

## See also

- [front](front.md): the first element
- [push_back](push_back.md), [pop_back](pop_back.md): the vector with an element more or less at the end
- [sgcl::immutable::vector\<T\>](README.md)
