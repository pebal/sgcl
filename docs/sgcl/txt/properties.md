# txt::properties

```cpp
#include "sgcl/txt/properties.h"
```

What a code point is, what it is worth, what it is written in, and how much room it takes. The tables are generated from Unicode 16.0.0 by `tools/unicode_tables.py`; ASCII is answered without them.

## The names

```cpp
category category_of(char32_t c) noexcept;   // the general category; category::unassigned when it has none
script script_of(char32_t c) noexcept;       // the script; script::unknown

bool is_alpha(char32_t c) noexcept;          // a letter: Lu, Ll, Lt, Lm, Lo (Go's unicode.IsLetter)
bool is_digit(char32_t c) noexcept;          // a decimal digit: Nd
bool is_alnum(char32_t c) noexcept;          // either
bool is_punct(char32_t c) noexcept;          // P*
bool is_mark(char32_t c) noexcept;           // M*: a combining, spacing or enclosing mark
bool is_control(char32_t c) noexcept;        // C0 and C1
bool is_format(char32_t c) noexcept;         // Cf: the joiner, the marks of direction, the soft hyphen
bool is_printable(char32_t c) noexcept;      // a letter, mark, number, punctuation mark or symbol, and the space
bool is_emoji(char32_t c) noexcept;          // Extended_Pictographic
bool is_space(char32_t c) noexcept;          // White_Space: core's unicode::is_space under this module's name
bool is_upper(char32_t c) noexcept;          // has a lower case form other than itself (unicode::is_upper)
bool is_lower(char32_t c) noexcept;          // has an upper case form other than itself (unicode::is_lower)

int numeric_value_of(char32_t c) noexcept;      // the value of a decimal digit, -1 when it is not one

size_t columns(char32_t c) noexcept;         // the cells it takes on a terminal: 0, 1 or 2
size_t columns(const string& text) noexcept; // the sum over the text; also slice<const char> and a C string

inline constexpr const char* version;        // the Unicode version of the tables, unicode::version
```

Every name is a `static constexpr` object, not a function ([the shape core's `unicode` uses](../core/utf8.md)): it takes a `char32_t` and refuses everything else — a `char` is a byte of UTF-8, an `int` is a multi-character literal — and it is passable where a predicate is asked for, so `s.runes().count_of(txt::is_alpha)` works. `columns` is the one with more than a code point in its interface: a [`string`](../core/string.md), a [`slice<const char>`](../core/slice.md) or a C string as well, and a `std::string_view` refused rather than left ambiguous, the module's texts being the library's, which hold what they point at.

## category

`enum class category : uint8_t`, the general category spelled out as ICU spells it, the UCD's abbreviation beside it:

| | | | |
|---|---|---|---|
| `unassigned` Cn | `uppercase_letter` Lu | `lowercase_letter` Ll | `titlecase_letter` Lt |
| `modifier_letter` Lm | `other_letter` Lo | `nonspacing_mark` Mn | `spacing_mark` Mc |
| `enclosing_mark` Me | `decimal_number` Nd | `letter_number` Nl | `other_number` No |
| `connector_punctuation` Pc | `dash_punctuation` Pd | `open_punctuation` Ps | `close_punctuation` Pe |
| `initial_punctuation` Pi | `final_punctuation` Pf | `other_punctuation` Po | `math_symbol` Sm |
| `currency_symbol` Sc | `modifier_symbol` Sk | `other_symbol` So | `space_separator` Zs |
| `line_separator` Zl | `paragraph_separator` Zp | `control` Cc | `format` Cf |
| `surrogate` Cs | `private_use` Co | | |

`unassigned` is zero, so a code point no range covers — two thirds of the space — answers it without a range of its own in the table. The order is the UCD's, so a run of categories is a range: `is_alpha` is one comparison of a pair, not five.

## script

`enum class script : uint16_t`, one enumerator per script of Unicode 16, generated with the tables: `latin`, `greek`, `cyrillic`, `han`, `hiragana`, `katakana`, `hangul`, `arabic`, `hebrew`, `devanagari`, `thai`, … and the three that are not writing systems, `common` (the digits, the punctuation, the space), `inherited` (a combining mark, which takes the script of what it sits on) and `unknown`, which is zero.

## What the answers mean

`is_alpha` is the letter categories, Go's `unicode.IsLetter` — not the Alphabetic property, which also holds `letter_number` and the marks that spell a vowel, and would make `is_alpha` true of a combining sign. `is_printable` is Go's `unicode.IsPrint`: a letter, mark, number, punctuation mark or symbol, and the space; the other separators, the controls and the unassigned are not. `is_emoji` is Extended_Pictographic, not the Emoji property, whose members are also `'#'`, `'*'` and the ten digits — and it is the property the segmentation needs, so the two agree on what one emoji is.

`columns` is East_Asian_Width: W and F take two cells, a combining or formatting code point none, and the rest one; class A (ambiguous — the Greek and Cyrillic letters the East Asian fonts draw wide) is one, as it is outside a CJK locale. This is the width of a monospaced cell, for a terminal and for aligning columns, and it is what `wrap()` and `truncate()` of the segmentation count. It is not the width of a glyph: a proportional font is measured by the one that draws it.

## Example

```cpp
#include "sgcl/sgcl.h"
#include <iostream>

using namespace sgcl;

// A table whose first column is text in any script, aligned by what it
// takes on the terminal rather than by how many bytes or code points it is
int main() {
    vector<string> words = {"żółw", "漢字", "Ελλάδα", "𝟛 emoji 😀", "naïve"};
    size_t width = 0;
    for (auto& w : words) {
        width = std::max(width, txt::columns(w));
    }
    for (auto& w : words) {
        string pad(width - txt::columns(w), ' ');
        std::cout << w << pad << " | " << w.size() << " bytes, " << w.rune_count() << " code points, "
                  << w.runes().count_of(txt::is_alpha) << " letters";
        if (w.runes().exists(txt::is_emoji)) {
            std::cout << ", an emoji";
        }
        std::cout << '\n';
    }
    string digits = "٣ ١ ٤";
    int sum = 0;
    for (char32_t c : digits.runes()) {
        sum += std::max(0, txt::numeric_value_of(c));
    }
    std::cout << digits << " sums to " << sum << ", written in "
              << (digits.runes().exists([](char32_t c) { return txt::script_of(c) == txt::script::arabic; }) ? "Arabic" : "?")
              << '\n';
    return 0;
}
```

The output, its first column as wide on the screen as it is here:

```
żółw       | 7 bytes, 4 code points, 4 letters
漢字       | 6 bytes, 2 code points, 2 letters
Ελλάδα     | 12 bytes, 6 code points, 6 letters
𝟛 emoji 😀 | 15 bytes, 9 code points, 5 letters, an emoji
naïve      | 6 bytes, 5 code points, 5 letters
٣ ١ ٤ sums to 8, written in Arabic
```

## The tables

| table | ranges | size |
|---|---|---|
| the general category | 3368 | 24.5 KB |
| the scripts | 979 | 8.3 KB |
| Extended_Pictographic | 78 | 0.4 KB |
| the decimal digits | 78 | 0.7 KB |
| East_Asian_Width W and F | 122 | 0.7 KB |

34.6 KB, compiled in; a question costs a binary search over sorted ranges, and ASCII costs none.

Each table is split at the end of the Basic Multilingual Plane. Seven ranges in ten lie below U+10000, and there a bound needs sixteen bits rather than thirty-two, which halves the entry; what is left goes into a second table with wide fields, and a range that straddles the boundary is cut in two, so neither search ever looks at the other. It costs one perfectly predictable branch and pays a third less memory to read: measured over a paragraph, five to ten per cent faster than one wide table, and a third smaller.

The generator checks every table against `unicodedata` code point by code point, and asserts that the UCD files it reads declare the same Unicode version as the Python that runs it.
