[sgcl](../../README.md) › [core](../README.md) › [array](../array.md)

# sgcl::array\<T, N\>::rbegin, crbegin

```cpp
/*(1)*/ constexpr reverse_iterator rbegin() noexcept;
/*(2)*/ constexpr const_reverse_iterator rbegin() const noexcept;
/*(3)*/ constexpr const_reverse_iterator crbegin() const noexcept;
```

Returns a reverse iterator to the last element, the first of the walk from the back: `reverse_iterator(end())`.

- (1) A reverse iterator that writes the elements.
- (2–3) A reverse iterator that reads them.

`array<T, 0>` has them all, and they are equal to `rend()`.

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
    array<string, 3> words = {"one", "two", "three"};
    for (auto it = words.crbegin(); it != words.crend(); ++it) {
        println("{}", *it);
    }

    *words.rbegin() = "last";
    println("{}", words);
}
```

Output:

```text
three
two
one
["one", "two", "last"]
```

## See also

- [rend, crend](rend.md): a reverse iterator to the end
- [begin, cbegin](begin.md): an iterator to the beginning
- [sgcl::array\<T, N\>](../array.md)
