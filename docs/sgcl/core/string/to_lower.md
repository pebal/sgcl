[sgcl](../../README.md) › [core](../README.md) › [string](../string.md)

# sgcl::string::to_lower

```cpp
basic_string to_lower() const;
```

Returns the string with every letter in lower case, by Unicode's simple case mapping, one code point to one
([unicode::to_lower](../unicode.md)): `"ŁÓDŹ"` to `"łódź"`. The other characters stay as they are. No language's
rules apply: `İ` lowers to `i`, as Unicode maps it; the full mapping and the rules of a language are
[txt::to_lower_full](../../txt/to_lower_full.md).

The length in bytes may change, as a mapped letter can take more or fewer bytes than its own: the Kelvin sign,
U+212A, three bytes, lowers to `k`, one. The same object when no letter changes (`"łódź"` has no upper-case
letter), so a result may be compared by `object()` as by `==`. A text all ASCII is mapped byte by byte, with no
decoding. A `wstring`, a `u16string` and a `u32string` are mapped a code point at a time too: in UTF-16 a surrogate
pair is one letter (Deseret's `u"𐐀"`, U+10400, lowers to `u"𐐨"`, U+10428), and a lone surrogate, which is no code
point, is left as it is, as an ill-formed byte of UTF-8 is. No letter's case is on the other side of U+FFFF, so a
wide string keeps its size.

## Parameters

None.

## Return value

The new string in lower case; this string's object when no letter changes.

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
    string city = "ŁÓDŹ";
    println("{}", city.to_lower());

    string lower = "łódź";
    println("{}", lower.to_lower().object() == lower.object());

    string kelvin = "\u212A";  // the Kelvin sign
    println("{} bytes, {} byte", kelvin.size(), kelvin.to_lower().size());
    println("{}", string("İSTANBUL").to_lower());

    wstring wide = L"ŁÓDŹ";
    println("{}", wide.to_lower() == L"łódź");

    u16string deseret = u"𐐀𐐁";  // two surrogate pairs
    println("{}", deseret.to_lower() == u"𐐨𐐩");
}
```

Output:

```text
łódź
true
3 bytes, 1 byte
istanbul
true
true
```

## See also

- [to_upper](to_upper.md): every letter in upper case
- [unicode](../unicode.md): the case of a code point
- [mixin::text](../mixin/text.md): `equal_fold`, the same letters in either case
- [txt::to_lower_full](../../txt/to_lower_full.md): the full case mapping and the rules of a language
- [sgcl::string](../string.md)
