[sgcl](../../../README.md) › [core](../../README.md) › [mixin](../README.md) › [text](../text.md)

# sgcl::mixin::text\<Derived, CharT, Traits\>::decode

```cpp
pair<char32_t, size_type> decode(size_type pos) const noexcept requires (sizeof(CharT) == 1);
```

Decodes the code point at the byte position `pos` of a UTF-8 text: `utf8::decode` of the text's bytes, Go's
`utf8.DecodeRuneInString(s[pos:])`. A byte that does not begin a valid sequence (a continuation byte, a truncated
sequence, an overlong encoding, a surrogate, a value past U+10FFFF) decodes as one byte and the replacement
character U+FFFD, as browsers and Go do.

Takes part only when `CharT` is one byte.

## Parameters

| Parameter | Description |
|---|---|
| `pos` | the byte position of the code point |

## Return value

The code point and the number of bytes it takes, 1 to 4: `{U+FFFD, 1}` for a byte that does not begin a valid
sequence, `{U+FFFD, 0}` for a `pos` at or past the end.

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
    string s = "żółw";
    for (size_t pos = 0; pos < s.size();) {
        auto [c, width] = s.decode(pos);
        println("{}: {:x}, width {}", pos, uint32_t(c), width);
        pos += width;
    }
    auto [inside, n] = s.decode(1);  // a continuation byte
    auto [end, none] = s.decode(s.size());
    println("{:x} {}, {:x} {}", uint32_t(inside), n, uint32_t(end), none);
}
```

Output:

```text
0: 17c, width 2
2: f3, width 2
4: 142, width 2
6: 77, width 1
fffd 1, fffd 0
```

## See also

- [runes](runes.md): the code points, walked one by one
- [at](at.md): the byte at a position
- [utf8, unicode, runes](../../utf8.md): `utf8::decode`, `utf8::decode_last`
- [sgcl::mixin::text\<Derived, CharT, Traits\>](../text.md)
