[sgcl](../../README.md) › [core](../README.md) › [utf8](../utf8.md)

# sgcl::utf8::width

```cpp
static constexpr size_t width(char32_t c) noexcept;
```

Returns the number of bytes the UTF-8 encoding of the code point `c` takes: 1 below U+0080, 2 below U+0800, 3 below
U+10000, 4 up to U+10FFFF; 0 for a value that is not a scalar value (a surrogate, or past U+10FFFF).

## Parameters

| Parameter | Description |
|---|---|
| `c` | the code point |

## Return value

1 to 4, or 0 when `c` is not a scalar value.

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
    println("{} {} {} {}", utf8::width(U'a'), utf8::width(U'ż'), utf8::width(U'€'), utf8::width(U'😀'));
    println("{} {}", utf8::width(char32_t(0xDC00)), utf8::width(char32_t(0x110000)));
}
```

Output:

```text
1 2 3 4
0 0
```

## See also

- [encode](encode.md): the encoding itself
- [valid](valid.md): whether a code point is a scalar value
- [sgcl::utf8](../utf8.md)
