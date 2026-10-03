[sgcl](../../README.md) › [core](../README.md) › [string](README.md)

# sgcl::string::end, cend

```cpp
const_iterator end() const noexcept;     // (1)
const_iterator cend() const noexcept;    // (2)
```

Returns an iterator past the last character: `data() + size()`, which points at the terminator. (2) is (1) under the
name of `std`. For the empty string it equals [begin()](begin.md).

## Parameters

None.

## Return value

An iterator past the last character.

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
    string entry = "name=alice";
    auto equals = std::find(entry.begin(), entry.end(), '=');
    if (equals != entry.end()) {
        string key(entry.begin(), equals);
        string value(equals + 1, entry.cend());
        println("{} {}", key, value);
    }
    println("{}", std::find(entry.begin(), entry.end(), '#') == entry.end());
}
```

Output:

```text
name alice
true
```

## See also

- [begin, cbegin](begin.md): an iterator to the first character
- [rend, crend](rend.md): a reverse iterator before the first character
- [sgcl::string](README.md)
