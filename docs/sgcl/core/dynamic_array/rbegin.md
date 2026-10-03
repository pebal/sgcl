[sgcl](../../README.md) › [core](../README.md) › [dynamic_array](../dynamic_array.md)

# sgcl::dynamic_array\<T\>::rbegin, crbegin

```cpp
reverse_iterator rbegin() noexcept;                 // (1)
const_reverse_iterator rbegin() const noexcept;     // (2)
const_reverse_iterator crbegin() const noexcept;    // (3)
```

Returns a reverse iterator to the last element, the first of the walk from the back: `reverse_iterator(end())`.

- (1) A reverse iterator that writes the elements.
- (2–3) A reverse iterator that reads them.

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
    dynamic_array<int> a = {1, 2, 3, 4};
    for (auto it = a.crbegin(); it != a.crend(); ++it) {
        println("{}", *it);
    }

    *a.rbegin() = 40;
    println("{}", a);
}
```

Output:

```text
4
3
2
1
[1, 2, 3, 40]
```

## See also

- [rend, crend](rend.md): a reverse iterator to the end
- [begin, cbegin](begin.md): an iterator to the beginning
- [sgcl::dynamic_array\<T\>](../dynamic_array.md)
