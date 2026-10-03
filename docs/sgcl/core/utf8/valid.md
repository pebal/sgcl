[sgcl](../../README.md) › [core](../README.md) › [utf8](README.md)

# sgcl::utf8::valid

```cpp
static constexpr bool valid(char32_t c) noexcept;            // (1)
static constexpr bool valid(std::string_view s) noexcept;    // (2)
```

1. Checks whether `c` is a Unicode scalar value: not a surrogate (U+D800 to U+DFFF) and not past U+10FFFF.
2. Checks whether every sequence of `s` is valid UTF-8: no byte that does not begin a sequence where one should
   begin, no truncated sequence, no overlong encoding, no surrogate, nothing past U+10FFFF. U+FFFD written in the
   text is valid, though it decodes as `replacement` as an invalid byte does.

## Parameters

| Parameter | Description |
|---|---|
| `c` | the code point |
| `s` | the bytes of the text |

## Return value

`true` when the code point, or every sequence of the text, is valid.

## Complexity

- (1) Constant.
- (2) Linear in `s.size()`, a run of ASCII eight bytes at a time.

## Exceptions

None.

## Notes

A string's `is_valid_utf8()` ([mixin::text](../mixin/text/README.md)) is (2) over its bytes. A string made of bytes that
came from outside is not checked when it is made; the strict decoding of [txt](../../txt/encoding.md) checks it.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    println("{} {}", utf8::valid(U'ż'), utf8::valid(char32_t(0xD800)));
    println("{} {} {}", utf8::valid("żółw"), utf8::valid("\xC0\xAF"), utf8::valid("\xEF\xBF\xBD"));
}
```

Output:

```text
true false
true false true
```

## See also

- [decode](decode.md): what an invalid sequence decodes as
- [sgcl::utf8](README.md)
