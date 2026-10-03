[sgcl](../../README.md) › [core](../README.md) › [utf8](README.md)

# sgcl::utf8::encode

```cpp
static constexpr size_t encode(char32_t c, char* out) noexcept;
```

Writes the UTF-8 encoding of the code point `c` into `out` and returns the number of bytes written, 1 to 4. A value
that is not a scalar value (a surrogate, or past U+10FFFF) is written as the encoding of `replacement`, three bytes.
Nothing else is written: no terminator.

## Parameters

| Parameter | Description |
|---|---|
| `c` | the code point |
| `out` | the buffer, with room for `max_width` bytes |

## Return value

The number of bytes written.

## Complexity

Constant.

## Exceptions

None.

## Notes

[encoded](../utf8-encoded/README.md) is the same encoding kept in a value with its length, which converts to a
`std::string_view`.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    char buffer[utf8::max_width];
    size_t n = utf8::encode(U'😀', buffer);
    println("{} {}", n, string(buffer, n));

    n = utf8::encode(char32_t(0xD800), buffer);  // a surrogate
    println("{} {}", n, utf8::decode(std::string_view(buffer, n)).first == utf8::replacement);
}
```

Output:

```text
4 😀
3 true
```

## See also

- [decode](decode.md): the reverse
- [width](width.md): the bytes an encoding takes
- [encoded](../utf8-encoded/README.md): the encoding as a value
- [sgcl::utf8](README.md)
