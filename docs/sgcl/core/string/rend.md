[sgcl](../../README.md) › [core](../README.md) › [string](../string.md)

# sgcl::string::rend, crend

```cpp
/*(1)*/ const_reverse_iterator rend() const noexcept;
/*(2)*/ const_reverse_iterator crend() const noexcept;
```

Returns a reverse iterator before the first character: `std::reverse_iterator` over [begin()](begin.md). (2) is (1)
under the name of `std`. For the empty string it equals [rbegin()](rbegin.md).

## Parameters

None.

## Return value

A reverse iterator before the first character.

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
    string path = "/usr/lib/libz.so";
    auto slash = std::find(path.rbegin(), path.rend(), '/');
    string name(slash.base(), path.end());
    println("{}", name);

    string bare = "readme";
    println("{}", std::find(bare.rbegin(), bare.crend(), '/') == bare.crend());
}
```

Output:

```text
libz.so
true
```

## See also

- [rbegin, crbegin](rbegin.md): a reverse iterator to the last character
- [end, cend](end.md): an iterator past the last character
- [sgcl::string](../string.md)
