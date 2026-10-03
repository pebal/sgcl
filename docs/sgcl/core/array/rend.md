[sgcl](../../README.md) › [core](../README.md) › [array](../array.md)

# sgcl::array\<T, N\>::rend, crend

```cpp
/*(1)*/ constexpr reverse_iterator rend() noexcept;
/*(2)*/ constexpr const_reverse_iterator rend() const noexcept;
/*(3)*/ constexpr const_reverse_iterator crend() const noexcept;
```

Returns a reverse iterator past the first element, the end of the walk from the back:
`reverse_iterator(begin())`. It may not be dereferenced.

- (1) The end of the reverse iterators that write the elements.
- (2–3) The end of the reverse iterators that read them.

`array<T, 0>` has them all, and they are equal to `rbegin()`.

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
#include <algorithm>

using namespace sgcl;

int main() {
    array<int, 6> a = {1, 7, 3, 7, 5, 0};
    auto last_seven = std::find(a.rbegin(), a.rend(), 7);
    println("the last 7 at {}", a.rend() - last_seven - 1);

    vector<int> backwards(a.rbegin(), a.rend());
    println("{}", backwards);
}
```

Output:

```text
the last 7 at 3
[0, 5, 7, 3, 7, 1]
```

## See also

- [rbegin, crbegin](rbegin.md): a reverse iterator to the beginning
- [end, cend](end.md): an iterator to the end
- [sgcl::array\<T, N\>](../array.md)
