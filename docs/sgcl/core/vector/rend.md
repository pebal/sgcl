[sgcl](../../README.md) › [core](../README.md) › [vector](../vector.md)

# sgcl::vector\<T\>::rend, crend

```cpp
/*(1)*/ reverse_iterator rend() noexcept;
/*(2)*/ const_reverse_iterator rend() const noexcept;
/*(3)*/ const_reverse_iterator crend() const noexcept;
```

Returns a reverse iterator past the first element, the end of the vector read backwards. It addresses no
element and must not be dereferenced.

- (1) A `reverse_iterator`, `std::reverse_iterator<iterator>`.
- (2–3) A `const_reverse_iterator`, `std::reverse_iterator<const_iterator>`.

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
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector v = {1, 2, 3};
    for (auto it = v.crbegin(); it != v.crend(); ++it) {
        println("{}", *it);
    }
}
```

Output:

```text
3
2
1
```

## See also

- [rbegin, crbegin](rbegin.md): a reverse iterator to the beginning
- [end, cend](end.md): an iterator to the end
- [sgcl::vector\<T\>](../vector.md)
