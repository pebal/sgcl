# sgcl::utf8, sgcl::unicode, sgcl::runes

```cpp
#include "sgcl/core/utf8.h"      // utf8; or "sgcl/core/core.h", "sgcl/sgcl.h"
#include "sgcl/core/unicode.h"   // unicode
#include "sgcl/core/slice.h"     // runes

namespace sgcl {
    struct utf8;       // the encoding: decode, encode, count, valid, width, decode_last, starts_rune, ascii_run, encoded
    struct unicode;    // the code point: to_lower, to_upper, is_upper, is_lower, equal_fold, is_space
    class runes;       // the code points of a text, decoded as they are walked: what runes() returns
}
```

The text of the library is Unicode, UTF-8 in a `char`: a [string](string.md) is bytes, its `size()` counts them, `s[i]` is a byte, and `find("ż")` searches bytes — as in Go, whose strings these follow. What Go puts in `unicode/utf8` and `unicode`, and what its `for i, r := range s` does, is here: `utf8` is the encoding, `unicode` the properties of a code point, and `runes` the code points of a text walked one by one. On these the text interface stands ([mixin::text](mixin/text.md)): `runes()`, `rune_count()`, `decode(pos)`, `is_valid_utf8()`; a `char32_t` as a character wherever a `char` is (`find(U'ż')`, `split(U'·')`, `trim(U"«»")`); `trim()` and `fields()` over Unicode white space; `to_lower()` and `to_upper()` by Unicode's case; `equal_fold`.

Nothing is rejected and nothing throws: a byte that is not the start of a valid sequence, a truncated sequence, an overlong encoding, a surrogate or a value past U+10FFFF is one code point, `utf8::replacement` (U+FFFD), as browsers and Go decode it; `is_valid_utf8()` says whether a text has any. On POSIX a path is bytes the system does not interpret, so a file name that is not UTF-8 goes through [`io`](../io/README.md) as it is.

## Rules

- **A run of ASCII, found eight bytes at a time.** `ascii_run` says how many bytes from a position are plain ASCII and `all_ascii` whether the whole text is. Most text is a long run of them, and for such a run the questions asked about a character mostly have one answer — two ASCII characters are always separate graphemes, a run of Latin letters is one word, the case of one is a single bit — so the callers walk the run in one step rather than one step a character. It is what `count()` stands on (0.60 ns a byte to 0.04 over ASCII, where Go's `RuneCountInString` reads 0.34) and what `to_lower()` and `to_upper()` stand on (4.4 ns a byte to 0.11) and what the boundaries and the case mappings of [txt](../txt/README.md) stand on. The test for a byte over 127 is one mask over a word of eight; there is no instruction in it a compiler would not write by itself.
- **A code point, not a character.** The unit is the Unicode scalar value, what `char32_t` holds. A user-perceived character may be several (a base letter and a combining accent, an emoji with a modifier); grapheme clusters, normalization and collation are not here. `rune_count()` counts code points.
- **Case is the simple mapping.** One code point to one, the `UnicodeData` mapping: what Go's `unicode.ToLower` and Java's `Character.toLowerCase` do. `ß` has no one-code-point upper case and stays `ß` (the full mapping, `SS`, changes the length); `İ` lowers to `i`; no language's rules (Turkish `ı`). The tables are generated from Unicode `unicode::version` by `tools/unicode_tables.py`.
- **White space is the White_Space property.** The six of the C locale (space, `\t`, `\n`, `\v`, `\f`, `\r`), U+0085, U+00A0, U+1680, U+2000 to U+200A, U+2028, U+2029, U+202F, U+205F, U+3000; not U+200B, the zero-width space, which is a format character.
- **A wide string's units are its code points.** `wstring`, `u16string` and `u32string` get the case and the white space per unit (a UTF-16 surrogate pair is left as it is); the UTF-8 members (`runes`, `decode`, the `char32_t` searches) are for `string` and `u8string`.
- **An `int` is no character.** `'ż'` in a UTF-8 source is a multi-character literal of type `int` (two bytes in one value), which a `find(char)` would cut to its last byte and find inside every `ź`, `ż` and `Ż`. The text interface deletes its `int` overloads, so `find('ż')` does not compile and the message says to write `U'ż'`. A `char` is still a byte: `find('a')` as before; `find(0)` is `find('\0')`.
- Every function of `utf8` and `unicode` is `constexpr` and takes bytes as a `std::string_view` (a string, a slice and a literal convert), holding nothing.

## Members

### utf8

```cpp
static constexpr char32_t replacement = U'\uFFFD';    // what an invalid byte decodes as
static constexpr size_t max_width = 4;                // the bytes of the longest encoding
static constexpr char32_t max_code_point = U'\U0010FFFF';

static constexpr bool valid(char32_t c) noexcept;                     // a scalar value: not a surrogate, not past U+10FFFF
static constexpr bool valid(std::string_view s) noexcept;             // every sequence valid
static constexpr size_t width(char32_t c) noexcept;                   // 1 to 4; 0 for no scalar value
static constexpr bool starts_rune(char b) noexcept;                   // not a continuation byte
static constexpr pair<char32_t, size_t> decode(std::string_view s, size_t i = 0) noexcept;   // the code point at s[i] and its bytes; {replacement, 1} for an invalid one, {replacement, 0} past the end
static constexpr pair<char32_t, size_t> decode_last(std::string_view s, size_t end = npos) noexcept;   // the last code point before `end` and its bytes
static constexpr size_t encode(char32_t c, char* out) noexcept;      // into out[max_width]: the bytes written; no scalar value encodes as the replacement
static constexpr size_t count(std::string_view s) noexcept;          // the code points, an invalid byte one each (a run of ASCII counted eight bytes at a time)
static constexpr size_t ascii_run(std::string_view s, size_t at = 0) noexcept;   // how many bytes from `at` are plain ASCII, read eight at a time
static constexpr bool all_ascii(std::string_view s) noexcept;
struct encoded { char bytes[max_width]; size_t size; constexpr encoded(char32_t); std::string_view view() const; operator std::string_view() const; };   // a code point as the bytes it is searched as
```

```cpp
static_assert(utf8::decode("żółw").first == U'ż' && utf8::decode("żółw").second == 2);
static_assert(utf8::count("żółw") == 4 && utf8::width(U'😀') == 4);
static_assert(utf8::decode("\xFF").first == utf8::replacement && !utf8::valid("\xFF"));
static_assert(utf8::encoded(U'€').view() == "€");
static_assert(utf8::ascii_run("abc żółw") == 4 && !utf8::all_ascii("żółw"));
for (size_t i = 0; i < s.size();) {                 // by hand: what runes() does
    auto [c, n] = utf8::decode(s, i);
    i += n;
}
```

### unicode

```cpp
static constexpr const char* version;                        // the Unicode version of the tables, "16.0.0"
char32_t to_lower(char32_t c) noexcept;      // the simple mapping; c when it has none
char32_t to_upper(char32_t c) noexcept;
bool is_upper(char32_t c) noexcept;          // has a lower case other than itself
bool is_lower(char32_t c) noexcept;          // has an upper case other than itself
bool equal_fold(char32_t a, char32_t b) noexcept;   // the same letter in either case: by to_lower or to_upper
bool is_space(char32_t c) noexcept;          // the White_Space property
```

ASCII is answered without the tables; another code point by a binary search over ranges (187 ranges to lower, 205 to upper: `A`–`Z` is one, `Ā ā Ă ă …` one with a stride of two).

The six are `static constexpr` objects, not functions — empty, trivially copyable, `constexpr` — and each takes a `char32_t` and **refuses everything else**: a `char` is a byte of UTF-8, not a code point (`is_space(s[0])` on a no-break space asked about `0xC2` and answered no, `to_lower(s[0])` gave `0xFFFFFFC2`), and an `int` is no character either (`'ż'` in a UTF-8 source is a multi-character literal). `char8_t`, `char16_t`, `wchar_t` and the plain integers are refused with them; write `s.decode(i).first`, a rune of `s.runes()`, or `U'ż'`. Objects rather than functions with deleted overloads, because a name whose overload set has more than one candidate cannot be passed where a predicate is deduced — `runes().count_of(unicode::is_upper)` has to keep working.

```cpp
static_assert(unicode::to_lower(U'Ł') == U'ł' && unicode::to_upper(U'ß') == U'ß' && unicode::to_lower(U'İ') == U'i');
static_assert(unicode::equal_fold(U'ς', U'Σ') && unicode::is_space(U'\u00A0') && !unicode::is_space(U'\u200B'));
string s = "ŁÓDŹ nad Wisłą";
size_t upper = s.runes().count_of(unicode::is_upper);        // 5: a name is a predicate
// unicode::is_space(s[0]);                                  // does not compile: a char is a byte, not a code point
```

### runes

```cpp
class runes : public mixin::enumerable<runes> {
    using value_type = char32_t;
    class iterator;                              // forward: char32_t operator*(), size_t pos() (the byte position), size_t width()
    runes() noexcept;                            // empty
    explicit runes(const slice<const char>& text) noexcept;
    iterator begin() const noexcept;  iterator end() const noexcept;
    bool empty() const noexcept;
    size_t count() const noexcept;               // walked, not stored
    const slice<const char>& text() const noexcept;
};
```

A range of `char32_t` over a [slice](slice.md) of the text, which holds the text's object for as long as the range lives: `for (char32_t c : string("żółw").runes())` walks a temporary safely. Decoded as it is walked, nothing stored; an invalid byte is one `utf8::replacement`. The iterator's `pos()` is the byte position of its code point, for the code that goes back to the bytes (Go's `for i, r := range s`). A range of the library ([mixin::enumerable](mixin/enumerable.md)): `contains`, `count_of`, `find_if`, `exists`, `for_each` on the code points, and a `const req::enumerable auto&` parameter takes it.

```cpp
string s = "żółw 😀";
size_t upper = s.runes().count_of(unicode::is_upper);      // 0
bool emoji = s.runes().exists([](char32_t c) { return c >= 0x1F600; });
for (auto it = s.runes().begin(); it != s.runes().end(); ++it) {
    std::cout << it.pos() << ':' << std::hex << *it << ' ';       // 0:17c 2:f3 4:142 6:77 7:20 8:1f600
}
```

### The text interface

On a `string` and a `slice<const char>`, from [mixin::text](mixin/text.md) — the page of each has the full list:

```cpp
runes runes() const noexcept;                        // the code points
size_type rune_count() const noexcept;               // utf8::count
pair<char32_t, size_type> decode(size_type pos) const noexcept;   // the code point at a byte position and its width
bool is_valid_utf8() const noexcept;
size_type find(char32_t c, size_type pos = 0) const noexcept;    // and rfind, contains, starts_with, ends_with: the code point as text
size_type find_first_of(std::u32string_view set, size_type pos = 0) const noexcept;   // and find_last_of, find_first_not_of, find_last_not_of: a set of code points
bool equal_fold(view_type s) const noexcept;         // the same letters in either case
// string: split(char32_t), replace(char32_t, char32_t), join(parts, char32_t), trim(std::u32string_view) and trim_left, trim_right
// string, slice: trim(), trim_left(), trim_right() over Unicode white space; fields() between runs of it; to_lower(), to_upper() by unicode::to_lower
```

## Example

```cpp
#include "sgcl/sgcl.h"
#include <iostream>

using namespace sgcl;

// The words of a line, in either case, counted by their first letter:
// the line is bytes, the letters are code points, and the map's keys
// are strings of one letter
int main() {
    string line = "\u00A0Łódź, Żywiec i Zakopane\u3000— łódka, żagiel, zamek.";
    map<string, int> by_letter;
    for (string_slice word : line.fields()) {                      // Unicode white space between the words
        word = word.trim(U",.—");                                   // a set of code points at both ends
        if (word.empty()) {
            continue;
        }
        auto [first, width] = word.decode(0);                      // the first code point and its bytes
        char lower[utf8::max_width];
        auto n = utf8::encode(unicode::to_lower(first), lower);
        ++by_letter[string(lower, n)];
    }
    for (auto& [letter, count] : by_letter) {
        std::cout << letter << ' ' << count << '\n';
    }
    std::cout << line.rune_count() << " code points in " << line.size() << " bytes, "
              << (line.to_lower().equal_fold(line.to_upper()) ? "one text in either case" : "?") << '\n';
    return by_letter.size() == 4 && by_letter["ł"] == 2 && by_letter["ż"] == 2 && by_letter["z"] == 2 && by_letter["i"] == 1 ? 0 : 1;
}
```

The output (a hash map: the letters in some order):

```
z 2
i 1
ż 2
ł 2
48 code points in 60 bytes, one text in either case
```

## See also

- [string](string.md), [slice](slice.md), [mixin::text](mixin/text.md): the text interface these stand under; [io::path](../io/path.md): `match` compares code points
- `tests/core/utf8.cpp`; `tools/unicode_tables.py`, which writes `sgcl/core/detail/unicode_tables.h`
