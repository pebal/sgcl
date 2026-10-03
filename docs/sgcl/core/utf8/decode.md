[sgcl](../../README.md) › [core](../README.md) › [utf8](../utf8.md)

# sgcl::utf8::decode

```cpp
static constexpr pair<char32_t, size_t> decode(std::string_view s, size_t i = 0) noexcept;
```

Decodes the code point that begins at the byte `i` of `s`, and returns it with the number of bytes it takes. A byte
that does not begin a valid sequence (a continuation byte, a truncated sequence, an overlong encoding, a surrogate,
a value past U+10FFFF) is one code point, `replacement`, of one byte, so a walk that adds the width to the position
always moves on. At or past the end of `s` the result is `replacement` of zero bytes.

## Parameters

| Parameter | Description |
|---|---|
| `s` | the bytes of the text |
| `i` | the position of the first byte of the code point |

## Return value

The code point and its width in bytes: `{c, 1}` to `{c, 4}` for a valid sequence, `{replacement, 1}` for an invalid
one, `{replacement, 0}` when `i >= s.size()`.

## Complexity

Constant.

## Exceptions

None.

## Notes

A walk over every code point is [runes](../runes.md), which calls this at each step; a string's `decode(pos)`
([mixin::text](../mixin/text.md)) is this over the string's bytes.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    std::string_view s = "ż\xFFz";
    for (size_t i = 0; i < s.size();) {
        auto [c, n] = utf8::decode(s, i);
        println("{}: U+{:04X}, {} bytes", i, uint32_t(c), n);
        i += n;
    }
    println("{}", utf8::decode(s, s.size()).second);
}
```

Output:

```text
0: U+017C, 2 bytes
2: U+FFFD, 1 bytes
3: U+007A, 1 bytes
0
```

## See also

- [decode_last](decode_last.md): the code point before a position
- [encode](encode.md): the reverse
- [runes](../runes.md): the code points of a text as a range
- [sgcl::utf8](../utf8.md)
