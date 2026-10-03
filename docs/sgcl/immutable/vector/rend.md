[sgcl](../../README.md) › [immutable](../README.md) › [vector](../vector.md)

# sgcl::immutable::vector\<T\>::rend, crend

```cpp
const_reverse_iterator rend() const noexcept;     // (1)
const_reverse_iterator crend() const noexcept;    // (2)
```

Returns a reverse iterator past the first element, the end of the vector read backwards. It may not be
dereferenced.

- (1–2) The same iterator, `std::reverse_iterator<const_iterator>`.

## Parameters

None.

## Return value

A reverse iterator past the first element.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/immutable.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    immutable::vector<int> v = {1, 2, 3, 4};
    immutable::vector reversed(v.crbegin(), v.crend());
    println("{} {}", reversed, v.crend() - v.crbegin());
}
```

Output:

```text
[4, 3, 2, 1] 4
```

## See also

- [rbegin, crbegin](rbegin.md): a reverse iterator to the beginning
- [end, cend](end.md): an iterator to the end
- [sgcl::immutable::vector\<T\>](../vector.md)
