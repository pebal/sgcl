[sgcl](../../../README.md) › [core](../../README.md) › [mixin](../README.md) › [text](../text.md)

# sgcl::mixin::text\<Derived, CharT, Traits\>::equal_fold

```cpp
bool equal_fold(view_type s) const noexcept;                         // (1)
template<size_t N>
bool equal_fold(const CharT (&s)[N]) const noexcept;                 // (2)
template<class P>
requires std::same_as<P, const CharT*> || std::same_as<P, CharT*>
bool equal_fold(P s) const noexcept;                                 // (3)
```

Checks whether the two texts are the same letters in either case: by the simple case folding of each code point
(`unicode::equal_fold`), Go's `strings.EqualFold`. In a UTF-8 text the two are decoded a code point at a time, so
they may differ in length (`"ſ"` against `"S"`), and an ill-formed byte, which decodes as U+FFFD, equals only the
same byte (never another ill-formed byte, nor U+FFFD written out, where Go's `strings.EqualFold` takes all of
them for one). A wide text is compared a code point at a time too, and the lengths must be equal, as no letter's
case is on the other side of U+FFFF: in UTF-16 a surrogate pair is one letter (`u"𐐀"` equals `u"𐐨"`, Deseret's
U+10400 and U+10428), and a lone surrogate, which is no code point, equals only the same unit.

1. With the text `s`: a string, a text slice, a `std::basic_string_view`.
2. With the characters of an array, a literal, up to its first NUL or its end, never past it.
3. With the characters at the pointer `s`, up to their NUL.

## Parameters

| Parameter | Description |
|---|---|
| `s` | the text to compare with |

## Return value

`true` when each code point of the text equals the one at the same place in `s`, or its lower or upper case form,
and the two end together; `false` otherwise.

## Complexity

Linear in the size of the texts: a pair of ASCII bytes is compared by a mask, without decoding; a pair of UTF-16
units is decoded only when the two differ and one is a surrogate.

## Exceptions

None.

## Notes

The folding is simple, one code point to one: `"Straße"` is not `"STRASSE"`, whose `"SS"` is two code points for
one.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string city = "Łódź";
    println("{} {}", city.equal_fold("ŁÓDŹ"), city.equal_fold("lodz"));
    string sisyphus = "Σίσυφος";
    println("{} {}", sisyphus.equal_fold("ΣΊΣΥΦΟΣ"), string("Straße").equal_fold("STRASSE"));
    println("{} {}", city.as_slice(0, 2).equal_fold(string("ł")), string("ſ").equal_fold("S"));

    wstring w = L"żółw";
    println("{}", w.equal_fold(L"ŻÓŁW"));

    u16string deseret = u"𐐀";  // one surrogate pair
    println("{} {}", deseret.equal_fold(u"𐐨"), deseret.equal_fold(u"𐐩"));
}
```

Output:

```text
true false
true false
true true
true
true false
```

## See also

- [compare](compare.md): the order of the characters, case and all
- [utf8, unicode, runes](../../utf8.md): `unicode::equal_fold`, `unicode::to_lower`
- [sgcl::mixin::text\<Derived, CharT, Traits\>](../text.md)
