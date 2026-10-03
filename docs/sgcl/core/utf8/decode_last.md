[sgcl](../../README.md) › [core](../README.md) › [utf8](../utf8.md)

# sgcl::utf8::decode_last

```cpp
static constexpr pair<char32_t, size_t> decode_last(std::string_view s,
                                                    size_t end = std::string_view::npos) noexcept;
```

Decodes the last code point of the bytes `s[0, end)`, and returns it with the number of bytes it takes: the step of a
walk backwards. It goes back over at most three continuation bytes to the byte that begins the sequence and decodes
from there. When the bytes before `end` are not a whole valid sequence, the result is `replacement` of one byte, so a
walk that subtracts the width always moves on; for an empty range it is `replacement` of zero bytes. An `end` past
the size is the size.

## Parameters

| Parameter | Description |
|---|---|
| `s` | the bytes of the text |
| `end` | the position just past the code point; the end of `s` by default |

## Return value

The code point and its width in bytes: `{c, 1}` to `{c, 4}`, `{replacement, 1}` when the bytes before `end` are not a
valid sequence, `{replacement, 0}` when `end` is 0 or `s` is empty.

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
    std::string_view s = "żółw";
    for (size_t end = s.size(); end > 0;) {
        auto [c, n] = utf8::decode_last(s, end);
        print("{}U+{:04X}", end == s.size() ? "" : " ", uint32_t(c));
        end -= n;
    }
    println("");
    println("{}", utf8::decode_last(s, 1).first == utf8::replacement);  // half of ż
}
```

Output:

```text
U+0077 U+0142 U+00F3 U+017C
true
```

## See also

- [decode](decode.md): the code point at a position
- [starts_rune](starts_rune.md): whether a byte begins a sequence
- [sgcl::utf8](../utf8.md)
