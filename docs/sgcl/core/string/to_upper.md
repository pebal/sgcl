[sgcl](../../README.md) › [core](../README.md) › [string](README.md)

# sgcl::string::to_upper

```cpp
basic_string to_upper() const;
```

Returns the string with every letter in upper case, by Unicode's simple case mapping, one code point to one
([unicode::to_upper](../unicode/README.md)): `"łódź"` to `"ŁÓDŹ"`. The other characters stay as they are, and so does
`ß`, whose upper case is two letters, `"SS"`, which only the full mapping gives. No language's rules apply; the full
mapping and the rules of a language are [txt::to_upper_full](../../txt/to_upper_full.md).

The length in bytes may change, as a mapped letter can take more or fewer bytes than its own: `ɐ`, two bytes, is `Ɐ`
in upper case, three. The same object when no letter changes (`"ŁÓDŹ"` has no lower-case letter), so a result may be
compared by `object()` as by `==`. A text all ASCII is mapped byte by byte, with no decoding. A `wstring`, a
`u16string` and a `u32string` are mapped a code point at a time too: in UTF-16 a surrogate pair is one letter
(Deseret's `u"𐐨"`, U+10428, is `u"𐐀"`, U+10400, in upper case), and a lone surrogate, which is no code point, is left
as it is, as an ill-formed byte of UTF-8 is. No letter's case is on the other side of U+FFFF, so a wide string keeps
its size.

## Parameters

None.

## Return value

The new string in upper case; this string's object when no letter changes.

## Complexity

Linear in the length of the string: a pass that finds whether a letter changes and the size of the result, then the
result written once, at that size.

## Exceptions

`length_error` when the result, longer in bytes, would pass `max_size()`.

## Notes

What a byte of the mapping costs, over ASCII and decoding a code point at a time, is on
[Benchmarks: Text](../benchmarks.md#text).

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string city = "łódź";
    println("{}", city.to_upper());

    string upper = "ŁÓDŹ";
    println("{}", upper.to_upper().object() == upper.object());

    println("{}", string("straße").to_upper());
    println("{} bytes, {} bytes", string("ɐ").size(), string("ɐ").to_upper().size());

    wstring wide = L"łódź";
    println("{}", wide.to_upper() == L"ŁÓDŹ");
}
```

Output:

```text
ŁÓDŹ
true
STRAßE
2 bytes, 3 bytes
true
```

## See also

- [to_lower](to_lower.md): every letter in lower case
- [unicode](../unicode/README.md): the case of a code point
- [mixin::text](../mixin/text/README.md): `equal_fold`, the same letters in either case
- [txt::to_upper_full](../../txt/to_upper_full.md): the full case mapping and the rules of a language
- [sgcl::string](README.md)
