[sgcl](../../README.md) › [core](../README.md) › [number_error](README.md)

# sgcl::number_error::offset

```cpp
constexpr size_t offset() const noexcept;
```

Returns the byte of the text where the reading stopped: 0 for an empty text and for one that does not begin as a
number of the type, the first byte after the number for a text with more after it and for a number out of the type's
range.

## Parameters

None.

## Return value

The offset in bytes from the start of the text.

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
    string text = "1500ms";
    auto n = parse<int>(text);
    size_t at = n.error().offset();
    println("the number {}, the unit {}", text.substr(0, at), text.substr(at));
}
```

Output:

```text
the number 1500, the unit ms
```

## See also

- [why](why.md): the reason
- [sgcl::number_error](README.md)
