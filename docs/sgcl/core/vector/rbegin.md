[sgcl](../../README.md) › [core](../README.md) › [vector](README.md)

# sgcl::vector\<T\>::rbegin, crbegin

```cpp
reverse_iterator rbegin() noexcept;                 // (1)
const_reverse_iterator rbegin() const noexcept;     // (2)
const_reverse_iterator crbegin() const noexcept;    // (3)
```

Returns a reverse iterator to the last element, the first of the vector read backwards; on an empty vector it is
equal to [rend()](rend.md).

- (1) A `reverse_iterator`, `std::reverse_iterator<iterator>`.
- (2–3) A `const_reverse_iterator`, `std::reverse_iterator<const_iterator>`.

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
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector<char> word = {'s', 'g', 'c', 'l'};
    string backwards(word.rbegin(), word.rend());
    println("{}", backwards);

    *word.rbegin() = 'L';
    println("{}", *word.crbegin());
}
```

Output:

```text
lcgs
L
```

## See also

- [rend, crend](rend.md): a reverse iterator to the end
- [begin, cbegin](begin.md): an iterator to the beginning
- [sgcl::vector\<T\>](README.md)
