[sgcl](../../README.md) › [core](../README.md) › [list](../list.md)

# sgcl::list\<T\>::rbegin, crbegin

```cpp
reverse_iterator rbegin() noexcept;                 // (1)
const_reverse_iterator rbegin() const noexcept;     // (2)
const_reverse_iterator crbegin() const noexcept;    // (3)
```

Returns a reverse iterator to the last element, the first of the list read backwards: `std::reverse_iterator` over
[end()](end.md). On an empty list it is equal to [rend()](rend.md).

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
    list l = {3, 2, 1};
    int step = 1;
    for (auto r = l.rbegin(); r != l.rend(); ++r) {
        *r *= step;  // from the back: 1 times 1, 2 times 2, 3 times 3
        ++step;
    }
    println("{}, the last {}", l, *l.crbegin());
}
```

Output:

```text
[9, 4, 1], the last 1
```

## See also

- [rend](rend.md): a reverse iterator to the end
- [begin](begin.md): an iterator to the beginning
- [sgcl::list\<T\>](../list.md)
