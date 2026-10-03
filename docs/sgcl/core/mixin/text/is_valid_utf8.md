[sgcl](../../../README.md) › [core](../../README.md) › [mixin](../README.md) › [text](../text.md)

# sgcl::mixin::text\<Derived, CharT, Traits\>::is_valid_utf8

```cpp
bool is_valid_utf8() const noexcept requires (sizeof(CharT) == 1);
```

Checks whether every sequence of a UTF-8 text is valid: `utf8::valid` of its bytes, Go's `utf8.ValidString`. A
continuation byte out of place, a truncated sequence, an overlong encoding, a surrogate or a value past U+10FFFF
makes the text invalid. The members that decode the text ([runes](runes.md), [decode](decode.md),
[rune_count](rune_count.md), the searches for a set of code points) reject nothing: they read an invalid byte as
U+FFFD.

Takes part only when `CharT` is one byte.

## Parameters

None.

## Return value

`true` when every sequence is valid, the empty text included; `false` otherwise.

## Complexity

Linear in the size of the text.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string good = "żółw 😀";
    string truncated = "\xC5";  // the first byte of ż alone
    string overlong = "\xC0\xAF";  // '/' in two bytes
    string surrogate = "\xED\xA0\x80";  // U+D800
    println("{} {}", good.is_valid_utf8(), truncated.is_valid_utf8());
    println("{} {}", overlong.is_valid_utf8(), surrogate.is_valid_utf8());
    println("{}", good.as_slice(0, 1).is_valid_utf8());
}
```

Output:

```text
true false
false false
false
```

## See also

- [decode](decode.md): the code point at a byte position, U+FFFD for an invalid byte
- [rune_count](rune_count.md): the number of code points
- [sgcl::mixin::text\<Derived, CharT, Traits\>](../text.md)
