[sgcl](../../README.md) › [txt](../README.md) › [regex_matches](README.md)

# sgcl::txt::regex_matches::end

```cpp
iterator end() const noexcept;
```

Returns the iterator past the last match: an iterator walked off the last match compares equal to it.

## Parameters

None.

## Return value

The end iterator.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    auto words = txt::regex("\\w+").all("raz dwa");
    int n = 0;
    for (auto it = words.begin(); it != words.end(); ++it) {
        ++n;
    }
    println("{}", n);
}
```

Output:

```text
2
```

## See also

- [begin](begin.md): an iterator to the first match
- [sgcl::txt::regex_matches](README.md)
