[sgcl](../README.md) › [core](README.md)

# sgcl::unicode

```cpp
#include "sgcl/core/unicode.h"   // or "sgcl/core.h"

namespace sgcl {
    struct unicode;
}
```

`sgcl::unicode` is the properties of a code point the text of the library needs: the simple case mappings and the
White_Space property, what Go's `unicode` package and Java's `Character` answer. A string's `to_lower()`,
`to_upper()`, `equal_fold()`, `trim()` and `fields()` stand on them ([string](string.md)), and the text a code point
comes from is decoded by [utf8](utf8.md).

The six names are `static constexpr` objects called as functions, not functions: each is empty, trivially copyable and
`constexpr`, and each takes a `char32_t` and refuses everything else. A `char` is a byte of UTF-8, not a code point:
`is_space(s[0])` on a no-break space would ask about the byte `0xC2` and answer no, and `to_lower(s[0])` would give
`0xFFFFFFC2`. An `int` is no character either, `'ż'` in a UTF-8 source being, where a compiler takes it, a
multi-character literal, and `char8_t`, `char16_t`, `wchar_t` and the plain integers are refused with them: a program
writes `s.decode(i).first`, a rune of `s.runes()`, or `U'ż'`. They are objects rather than functions with deleted
overloads because a name whose overload set has more than one candidate cannot be passed where a predicate is deduced,
and `s.runes().count_of(unicode::is_upper)` has to work.

## Rules

- The case is the simple mapping, one code point to one, Unicode's `UnicodeData` mapping: what Go's
  `unicode.ToLower` and Java's `Character.toLowerCase` do. `ß` has no upper case of one code point and stays `ß`
  (the full mapping, `SS`, changes the length: [txt::to_upper_full](../txt/to_upper_full.md)); `İ` lowers to `i`; no
  language's rules apply (the Turkish `ı`).
- The white space is the White_Space property: the six of the C locale (space, `\t`, `\n`, `\v`, `\f`, `\r`),
  U+0085, U+00A0, U+1680, U+2000 to U+200A, U+2028, U+2029, U+202F, U+205F and U+3000; not U+200B, the zero-width
  space, which is a format character.
- A code point of the Basic Multilingual Plane, ASCII included, is answered by a two-stage table: the block of 32
  code points it falls in names a block of differences, shared by the blocks that are the same, so a case is two
  reads and an addition, without a branch (5.6 KB to lower, 6.2 KB to upper). A code point above the plane is
  found by a binary search over 11 ranges (Deseret, Osage, Adlam and the others). The tables are generated from
  the Unicode version `version` names by `tools/unicode_tables.py`.
- Every call is `constexpr` and `noexcept`, and holds nothing.

## Member objects

| Constant | Value | Description |
|---|---|---|
| `version` | `"16.0.0"` | the Unicode version of the tables, `static constexpr const char*` |

## Member functions

#### Case

| Function | Description |
|---|---|
| [to_lower](unicode/to_lower.md) | the lower case of a code point |
| [to_upper](unicode/to_upper.md) | the upper case of a code point |
| [is_upper](unicode/is_upper.md) | checks whether a code point has a lower case other than itself |
| [is_lower](unicode/is_lower.md) | checks whether a code point has an upper case other than itself |
| [equal_fold](unicode/equal_fold.md) | checks whether two code points are the same letter in either case |

#### White space

| Function | Description |
|---|---|
| [is_space](unicode/is_space.md) | checks whether a code point is white space |

## Complexity

Constant: two reads of a table in the Basic Multilingual Plane, a binary search over 11 ranges above it.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string s = "ŁÓDŹ nad Wisłą";
    println("{} upper, {} lower", s.runes().count_of(unicode::is_upper),
            s.runes().count_of(unicode::is_lower));

    println("{} {}", s.runes().count_of(unicode::is_space), unicode::is_space(U'\u3000'));
    println("{}", unicode::to_lower(U'Ł') == U'ł' && unicode::equal_fold(U'ą', U'Ą'));
    println("Unicode {}", unicode::version);
}
```

Output:

```text
5 upper, 7 lower
2 true
true
Unicode 16.0.0
```

## See also

- [utf8](utf8.md): the encoding a code point is decoded from
- [runes](runes.md): the code points of a text, where these are predicates
- [to_lower](string/to_lower.md), [to_upper](string/to_upper.md), [trim](string/trim.md),
  [fields](string/fields.md): what a string does with them
- [txt::to_lower_full](../txt/to_lower_full.md), [txt::to_upper_full](../txt/to_upper_full.md): the full case mapping and the rules of a language
