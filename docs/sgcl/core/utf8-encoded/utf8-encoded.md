[sgcl](../../README.md) › [core](../README.md) › [utf8](../utf8/README.md) › [encoded](README.md)

# sgcl::utf8::encoded::encoded

```cpp
constexpr encoded(char32_t c) noexcept;
```

Encodes the code point `c` into the value: `bytes` holds its encoding, as [utf8::encode](../utf8/encode.md) writes
it, the bytes after it zero, and `size` the number of bytes. A value that is not a scalar value is encoded as
`utf8::replacement`. The constructor is not `explicit`: a `char32_t` converts to an `encoded` where one is taken.

## Parameters

| Parameter | Description |
|---|---|
| `c` | the code point |

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
    constexpr utf8::encoded dot(U'·');
    println("{} {}", dot.size, dot.view() == "·");

    utf8::encoded bad(char32_t(0x110000));
    println("{} {}", bad.size, bad.view() == "\uFFFD");
}
```

Output:

```text
2 true
3 true
```

## See also

- [view, operator std::string_view](view.md): the bytes as a view
- [utf8::encode](../utf8/encode.md): the encoding into a buffer of the caller's
- [sgcl::utf8::encoded](README.md)
