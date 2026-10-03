[sgcl](../../README.md) › [core](../README.md) › [array](../array.md)

# sgcl::array\<T, N\>::end, cend

```cpp
/*(1)*/ constexpr iterator end() noexcept;
/*(2)*/ constexpr const_iterator end() const noexcept;
/*(3)*/ constexpr const_iterator cend() const noexcept;
```

Returns an iterator past the last element, `begin() + N`. It may not be dereferenced.

- (1) The end of the iterators that write the elements.
- (2–3) The end of the iterators that read them.

## Parameters

None.

## Return value

An iterator past the last element.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <algorithm>

using namespace sgcl;

int main() {
    array<int, 6> a = {4, 8, 15, 16, 23, 42};
    println("{}", a.end() - a.begin());

    auto it = std::find(a.cbegin(), a.cend(), 99);
    println("{}", it == a.cend());

    for (auto p = a.begin(); p != a.end(); p += 2) {
        println("{}", *p);
    }
}
```

Output:

```text
6
true
4
15
23
```

## See also

- [begin, cbegin](begin.md): an iterator to the beginning
- [rend, crend](rend.md): a reverse iterator to the end
- [sgcl::array\<T, N\>](../array.md)
