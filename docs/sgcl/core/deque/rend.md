[sgcl](../../README.md) › [core](../README.md) › [deque](../deque.md)

# sgcl::deque\<T\>::rend, crend

```cpp
reverse_iterator rend() noexcept;                 // (1)
const_reverse_iterator rend() const noexcept;     // (2)
const_reverse_iterator crend() const noexcept;    // (3)
```

Returns a reverse iterator past the first element, the end of the walk from the back:
`reverse_iterator(begin())`. It is not dereferenced; on an empty deque it equals [rbegin](rbegin.md).

- (1) A `reverse_iterator`.
- (2–3) A `const_reverse_iterator`.

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
    deque<int> d = {4, 8, 15, 16, 23, 42};
    auto odd = std::find_if(d.crbegin(), d.crend(), [](int x) { return x % 2 != 0; });
    println("the last odd: {}, {} from the back", *odd, odd - d.crbegin());
}
```

Output:

```text
the last odd: 23, 1 from the back
```

## See also

- [rbegin, crbegin](rbegin.md): a reverse iterator to the beginning
- [end, cend](end.md): an iterator to the end
- [sgcl::deque\<T\>](../deque.md)
