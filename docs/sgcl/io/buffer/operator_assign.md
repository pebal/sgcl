[sgcl](../../README.md) › [io](../README.md) › [buffer](../buffer.md)

# sgcl::io::buffer::operator=

```cpp
/*(1)*/ buffer& operator=(const buffer&) noexcept = default;
/*(2)*/ buffer& operator=(buffer&&) noexcept = default;
```

Makes this handle the same buffer as the other: the word is copied (1) or moved (2), and the state this handle held
is left to the collector, or to the other handles that still hold it. No byte is copied.

## Parameters

One, unnamed: the handle whose buffer this one becomes.

## Return value

`*this`.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffer current("first");
    io::buffer kept = current;
    current = io::buffer("second");
    println("{} {} {}", current.text(), kept.text(), current == kept);
}
```

Output:

```text
second first false
```

## See also

- [(constructor)](buffer.md)
- [sgcl::io::buffer](../buffer.md)
