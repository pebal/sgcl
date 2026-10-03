[sgcl](../../README.md) › [core](../README.md) › [list](../list.md)

# sgcl::list\<T\>::rend, crend

```cpp
/*(1)*/ reverse_iterator rend() noexcept;
/*(2)*/ const_reverse_iterator rend() const noexcept;
/*(3)*/ const_reverse_iterator crend() const noexcept;
```

Returns the reverse iterator past the first element, the end of the list read backwards: `std::reverse_iterator`
over [begin()](begin.md). It is not to be dereferenced.

## Parameters

None.

## Return value

The reverse iterator past the first element.

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
    list l = {1, 2, 3, 2, 1};
    auto last_two = std::find(l.rbegin(), l.rend(), 2);  // the last 2, searched from the back
    println("{}", std::distance(last_two, l.rend()));

    list<int> empty;
    println("{}", empty.rbegin() == empty.rend());
}
```

Output:

```text
4
true
```

## See also

- [rbegin](rbegin.md): a reverse iterator to the beginning
- [end](end.md): an iterator to the end
- [sgcl::list\<T\>](../list.md)
