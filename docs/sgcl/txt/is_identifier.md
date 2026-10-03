[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::is_identifier

```cpp
#include "sgcl/txt/identifier.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    /*(1)*/ bool is_identifier(const string& text) noexcept;
    /*(2)*/ bool is_identifier(const string& text, program_syntax_t) noexcept;
}
```

Checks whether the whole text is an identifier by rule R1 of [UAX #31](https://www.unicode.org/reports/tr31/): a
code point that may begin one ([is_identifier_start](is_identifier_start.md)) followed by code points that may
continue one ([is_identifier_continue](is_identifier_continue.md)), with rule R1a for the two joiners. A variable of
a language, a key of a configuration file, the label of a domain.

1. By `XID_Start` and `XID_Continue`.
2. By the [program_syntax](program_syntax_t.md) profile: `_` and `$` as well.

**Rule R1a, the two joiners.** `U+200D ZERO WIDTH JOINER` and `U+200C ZERO WIDTH NON-JOINER` are formatting code
points and are in neither `XID` set, and the Indic languages cannot be written without them: `क्ष` is a conjunct and
`क्‍ष` with a joiner in it is not, and the two are different words. A joiner is allowed after a virama, a mark of
combining class 9; a non-joiner is allowed there too and also where it breaks a cursive join that would otherwise
happen, between a letter that joins to the left and one that joins to the right. That is the same line
[RFC 5892](https://www.rfc-editor.org/rfc/rfc5892) draws for domain names. Anywhere else they are refused:
`"a" ZWJ "b"` is not an identifier.

- (1–2) An empty text is not an identifier. Nothing here says the name is a good idea:
  [restriction_level_of](restriction_level_of.md) is asked for that.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text, UTF-8; an invalid byte is `U+FFFD`, which is no identifier's |

## Return value

`true` when the text is an identifier.

## Complexity

Linear in the length of the text.

## Exceptions

None.

## Notes

- **This is not IDNA.** A domain label has rules of its own, the length, the hyphens in the third and fourth places,
  the Punycode, and they are [idna](idna.md)'s.
- A program that only asks `is_identifier` links the two `XID` sets, 8.8 KB, and the joining types rule R1a needs,
  4.2 more.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    for (auto s : {"wartość", "_name", "2name", "na me", ""}) {
        println("[{}]: identifier {}, with the profile {}", s, txt::is_identifier(s),
                txt::is_identifier(s, txt::program_syntax));
    }
    println("{} {}", txt::is_identifier("क\u094D\u200Dष"), txt::is_identifier("a\u200Db"));
}
```

Output:

```text
[wartość]: identifier true, with the profile true
[_name]: identifier false, with the profile true
[2name]: identifier false, with the profile false
[na me]: identifier false, with the profile false
[]: identifier false, with the profile false
true false
```

## See also

- [is_identifier_start](is_identifier_start.md), [is_identifier_continue](is_identifier_continue.md): one code point
- [nfkc_casefold](nfkc_casefold.md): the form identifiers are compared in
- [restriction_level_of](restriction_level_of.md): whether the name is a safe one
- [txt](README.md)
