[sgcl](../../README.md) › [core](../README.md) › [slice](../slice.md)

# sgcl::slice\<T\>::rend, crend

```cpp
/*(1)*/ reverse_iterator rend() const noexcept;
/*(2)*/ const_reverse_iterator crend() const noexcept;
```

A reverse iterator past the first element, the end of the reversed slice.

## Parameters

None.

## Return value

A reverse iterator to the end of the reversed slice.

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
    string text = "abc";
    string_slice s = text;
    for (auto it = s.rbegin(); it != s.rend(); ++it) {
        print("{}", *it);
    }
    println();
}
```

Output:

```text
cba
```

## See also

- [rbegin, crbegin](rbegin.md): the reverse iterator to the beginning
- [sgcl::slice\<T\>](../slice.md)
