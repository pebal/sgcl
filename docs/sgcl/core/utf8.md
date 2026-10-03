[sgcl](../README.md) › [core](README.md)

# sgcl::utf8

```cpp
#include "sgcl/core/utf8.h"   // or "sgcl/core.h"

namespace sgcl {
    struct utf8;
}
```

`sgcl::utf8` is the encoding of Unicode in bytes, the functions of Go's `unicode/utf8`: a code point decoded at a
position, encoded into up to four bytes, and the code points of a text counted and validated. It is a struct of
static functions and constants, holding nothing.

The text of the library is Unicode, UTF-8 in a `char`: a [string](string.md) is bytes, its `size()` counts them,
`s[i]` is a byte, and `find("ż")` searches bytes, as in Go, whose strings these follow. `utf8` is the encoding,
[unicode](unicode.md) the properties of a code point, and [runes](runes.md) the code points of a text walked one by
one, what Go's `for i, r := range s` does. On these the text interface of a string and of a text slice stands
([mixin::text](mixin/text.md)): `runes()`, `rune_count()`, `decode(pos)`, `is_valid_utf8()`, a `char32_t` as a
character wherever a `char` is, a set of code points as a `std::u32string_view`, `trim()` and `fields()` over Unicode
white space, `to_lower()` and `to_upper()` by Unicode's case, and `equal_fold`.

Nothing is rejected and nothing throws: a byte that is not the start of a valid sequence, a truncated sequence, an
overlong encoding, a surrogate or a value past U+10FFFF is one code point, `replacement` (U+FFFD), as browsers and Go
decode it; `valid` says whether a text has any. On POSIX a path is bytes the system does not interpret, so a file name
that is not UTF-8 goes through [io](../io/README.md) as it is.

## Rules

- Every function is `constexpr` and `noexcept`, and takes the bytes as a `std::string_view`: a string, a text slice
  and a literal convert to it. Nothing is kept: a function looks and returns.
- The unit is the code point, the Unicode scalar value a `char32_t` holds, not a character as a reader sees one: a
  base letter and a combining accent, or an emoji with a modifier, are several. Grapheme clusters, normalization and
  collation are in [txt](../txt/README.md).
- A run of ASCII is read eight bytes at a time. `ascii_run` says how many bytes from a position are plain ASCII and
  `all_ascii` whether the whole text is. Most text is a long run of them, and for such a run the questions asked
  about a character mostly have one answer: two ASCII characters are always separate graphemes, a run of Latin
  letters is one word, the case of one is a single bit. So the callers walk the run in one step rather than one step
  a character: `count`, `to_lower()` and `to_upper()` of a string, the boundaries and the case mappings of
  [txt](../txt/README.md). The test for a byte over 127 is one mask over a word of eight, with no instruction in it
  a compiler would not write by itself.

## Member types

| Type | Definition |
|---|---|
| [encoded](utf8-encoded.md) | a code point as the bytes of its encoding, with their number |

## Member objects

| Constant | Value | Description |
|---|---|---|
| `replacement` | `U'\uFFFD'` | what an invalid byte decodes as, and what a value that is not a scalar value encodes as, `static constexpr char32_t` |
| `max_width` | `4` | the bytes of the longest encoding, `static constexpr size_t` |
| `max_code_point` | `U'\U0010FFFF'` | the last code point, `static constexpr char32_t` |

## Member functions

#### Decoding

| Function | Description |
|---|---|
| [decode](utf8/decode.md) | the code point at a position and the bytes it takes |
| [decode_last](utf8/decode_last.md) | the last code point before a position and the bytes it takes |
| [starts_rune](utf8/starts_rune.md) | checks whether a byte begins a sequence |

#### Encoding

| Function | Description |
|---|---|
| [encode](utf8/encode.md) | writes the encoding of a code point |
| [width](utf8/width.md) | the bytes the encoding of a code point takes |

#### Validation and counting

| Function | Description |
|---|---|
| [valid](utf8/valid.md) | checks whether a code point is a scalar value, or whether every sequence of a text is valid |
| [count](utf8/count.md) | the code points of a text |
| [ascii_run](utf8/ascii_run.md) | how many bytes from a position are plain ASCII |
| [all_ascii](utf8/all_ascii.md) | checks whether a text is all ASCII |

## Complexity

`decode`, `decode_last`, `encode`, `width`, `starts_rune` and `valid` of a code point: constant. `valid` of a text,
`count`, `ascii_run` and `all_ascii`: linear in the bytes read, a run of ASCII eight bytes at a time. The costs are on
[Benchmarks: Text](benchmarks.md#text).

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

// The words of a line, in either case, counted by their first letter:
// the line is bytes, the letters are code points, and the keys of the
// map are strings of one letter
int main() {
    string line = "\u00A0Łódź, Żywiec i Zakopane\u3000— łódka, żagiel, zamek.";
    sorted_map<string, int> by_letter;
    for (string_slice word : line.fields()) {  // Unicode white space between the words
        word = word.trim(U",.—");  // a set of code points at both ends
        if (word.empty()) {
            continue;
        }
        auto [first, width] = word.decode(0);  // the first code point and its bytes
        char lower[utf8::max_width];
        auto n = utf8::encode(unicode::to_lower(first), lower);
        ++by_letter[string(lower, n)];
    }
    for (auto& [letter, count] : by_letter) {
        println("{} {}", letter, count);
    }
    println("{} code points in {} bytes", line.rune_count(), line.size());
}
```

Output:

```text
i 1
z 2
ł 2
ż 2
48 code points in 60 bytes
```

## See also

- [unicode](unicode.md): the case and the white space of a code point
- [runes](runes.md): the code points of a text as a range
- [string](string.md), [slice](slice.md), [mixin::text](mixin/text.md): the text interface these stand under
- [io::path](../io/path.md): `match` compares code points
- [txt](../txt/README.md): graphemes, words, normalization, the full case mapping
