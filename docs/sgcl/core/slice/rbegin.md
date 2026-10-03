[sgcl](../../README.md) › [core](../README.md) › [slice](../slice.md)

# sgcl::slice\<T\>::rbegin, crbegin

```cpp
reverse_iterator rbegin() const noexcept;           // (1)
const_reverse_iterator crbegin() const noexcept;    // (2)
```

A reverse iterator to the last element, `std::reverse_iterator` over the slice's pointers.

## Parameters

None.

## Return value

A reverse iterator to the beginning of the reversed slice.

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
    vector v = {1, 2, 3};
    slice<const int> s = v;
    vector<int> backwards(s.crbegin(), s.crend());
    println("{} {}", *s.rbegin(), backwards);
}
```

Output:

```text
3 [3, 2, 1]
```

## See also

- [rend, crend](rend.md): the reverse iterator to the end
- [sgcl::slice\<T\>](../slice.md)
