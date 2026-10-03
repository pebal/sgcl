[sgcl](../../README.md) › [immutable](../README.md) › [vector](README.md)

# sgcl::immutable::vector\<T\>::rbegin, crbegin

```cpp
const_reverse_iterator rbegin() const noexcept;     // (1)
const_reverse_iterator crbegin() const noexcept;    // (2)
```

Returns a reverse iterator to the last element, the first of the vector read backwards; on an empty vector it is
equal to [rend()](rend.md).

- (1–2) The same iterator, `std::reverse_iterator<const_iterator>`.

## Parameters

None.

## Return value

A reverse iterator to the last element.

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
    immutable::vector<char> word = {'t', 'r', 'i', 'e'};
    string backwards(word.rbegin(), word.rend());
    println("{}", backwards);
    println("{}", *word.crbegin());
}
```

Output:

```text
eirt
e
```

## See also

- [rend, crend](rend.md): a reverse iterator to the end
- [begin, cbegin](begin.md): an iterator to the beginning
- [sgcl::immutable::vector\<T\>](README.md)
