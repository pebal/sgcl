[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::is_identifier_start

```cpp
#include "sgcl/txt/identifier.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    inline constexpr /* unspecified */ is_identifier_start {};   // called as is_identifier_start(c)

    /*(1)*/ constexpr bool operator()(char32_t c) const noexcept;
    /*(2)*/ constexpr bool operator()(char32_t c, program_syntax_t) const noexcept;
    /*(3)*/ template<class T, class... Rest> requires (!std::same_as<T, char32_t>)
            constexpr bool operator()(T, Rest...) const noexcept = delete;
}
```

An object called as a function, `txt::is_identifier_start(c)`, and passed as a predicate. It takes a `char32_t` and
refuses everything else, as the [module's rules](README.md) say of every question about a code point; the profile is
a second argument rather than a second name, so the one object is still passable as a predicate of one code point.

1. Checks whether the code point `c` may begin an identifier: the `XID_Start` property of
   [UAX #31](https://www.unicode.org/reports/tr31/), the letters and the letter numbers.
2. The same with the [program_syntax](program_syntax_t.md) profile: `_` and `$` begin an identifier too.
3. Every other first argument is refused: a `char`, an `int`, a `char8_t`, a `char16_t`, a `wchar_t`.

`XID_Start` and not `ID_Start`. **The X is the whole point**: the plain pair is not closed under normalization, so a
text that is an identifier can stop being one when it is put into NFKC. `U+037A GREEK YPOGEGRAMMENI` is `ID_Start`
and normalizes to a space and an iota; `U+309B KATAKANA-HIRAGANA VOICED SOUND MARK` does the same; `U+0E33 THAI
CHARACTER SARA AM` normalizes to a mark and a vowel, and a mark may not begin a name. Twenty-three code points are
`ID_Start` and not `XID_Start`, nineteen are `ID_Continue` and not `XID_Continue`, and they are exactly the ones that
would break that way. Taking them out is what lets a compiler compare two names in a normal form and a linker carry
one.

## Parameters

| Parameter | Description |
|---|---|
| `c` | the code point |

## Return value

`true` when `c` may begin an identifier.

## Complexity

Constant: two reads of a table below U+10000, a binary search over sorted ranges above.

## Exceptions

None.

## Notes

`XID_Start` and `XID_Continue` are held over **every one of the 1 114 112 code points** to Python's own tables
rather than the UCD's (`str.isidentifier()` is the first with the underscore added and `"a" + c` is the second), the
generator asserting that the two sources agree before it writes either.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    for (char32_t c : {U'a', U'ż', U'_', U'$', U'7', U'ͺ'}) {
        println("U+{:04X}: {} {}", uint32_t(c), txt::is_identifier_start(c),
                txt::is_identifier_start(c, txt::program_syntax));
    }
}
```

Output:

```text
U+0061: true true
U+017C: true true
U+005F: false true
U+0024: false true
U+0037: false false
U+037A: false false
```

## See also

- [is_identifier_continue](is_identifier_continue.md): whether a code point may stand further along
- [is_identifier](is_identifier.md): a whole text
- [txt](README.md)
