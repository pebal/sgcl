[sgcl](../../README.md) › [core](../README.md) › [string](README.md)

# sgcl::string::split

```cpp
pieces split(view_type sep, size_type max_parts = 0) const;                       // (1)
pieces split(CharT sep, size_type max_parts = 0) const noexcept;                  // (2)
template<size_t N>
pieces split(const CharT (&sep)[N], size_type max_parts = 0) const;               // (3)
template<class P>
requires std::same_as<P, const CharT*> || std::same_as<P, CharT*>
pieces split(P sep, size_type max_parts = 0) const;                               // (4)
pieces split(const basic_string& sep, size_type max_parts = 0) const noexcept;    // (5)
pieces split(char32_t sep, size_type max_parts = 0) const noexcept                // (6)
    requires (!std::same_as<CharT, char32_t>);
pieces split(int, size_type = 0) const = delete;                                  // (7)
```

Splits the string at the occurrences of `sep`: the pieces between them, in order, as a range of slices into the
string, [pieces](../string-pieces/README.md).

1. Splits at the characters of a view.
2. Splits at a character.
3. Splits at the characters of an array, a literal, read up to its first NUL or its end, whichever comes first.
4. Splits at the characters a pointer points to, up to their NUL. Takes part only for `CharT*` and `const CharT*`:
   `nullptr` and every other pointer do not compile.
5. Splits at a string, which the range holds as it is.
6. Splits at a code point, at its units: the bytes of its UTF-8 in a `string` and a `u8string`, `split(U'·')`, one
   unit or a surrogate pair in a `u16string`. A value that is no code point (a surrogate, past U+10FFFF) occurs
   nowhere, not as the U+FFFD it would be written as: the string is one piece. For every string but a `u32string`,
   whose (2) takes a code point.
7. Deleted. `'ż'` in a UTF-8 source is, where a compiler takes it, a multi-character literal of type `int`, two bytes in
   one value, which a `char` separator would cut to its last byte, so `split('ż')` does not compile; `split(U'ż')` is
   (6).

An empty piece stands where two separators meet and where a separator begins or ends the string: Go's
`strings.Split` keeps them all, Java's `split` drops the empty pieces at the end. A string in which `sep` does not
occur is one piece, the whole string. An empty `sep` splits into the code points one by one, as Go's
`strings.Split(s, "")` does: in UTF-8 a piece is the bytes of one code point (an invalid byte a piece of its own),
in UTF-16 a unit or a surrogate pair. An empty string splits
into nothing, where Go's `strings.Split` gives one empty piece. With `max_parts`, there are at most that many
pieces, and the last holds the rest of the string, separators and all; 0 is no limit.

Nothing is searched by the call: each piece is found as the walk reaches it, one search per piece, and nothing is
allocated. Each piece is a [slice](../slice/README.md) that holds the string's object, valid on its own wherever it is
kept. The separator is copied into the range, one of up to 16 bytes inside it and a longer one in a string of its
own, so a temporary separator in the head of a range-for cannot dangle.

## Parameters

| Parameter | Description |
|---|---|
| `sep` | the separator: a text, a character or a code point |
| `max_parts` | the most pieces to give, the last holding the rest of the string; 0, the default, is no limit |

## Return value

The range of the pieces, a [pieces](../string-pieces/README.md) holding this string, the separator and the limit; its
elements are `string_slice`s, `slice<const CharT>`.

## Complexity

Constant, plus a copy of a separator longer than 16 bytes into a string of its own (1, 3–4). The walk of the range is
linear in the length of the string, one search for the separator per piece.

## Exceptions

- (1), (3–4) `length_error` when a separator longer than 16 bytes, kept in a string of its own, would pass
  `max_size()`.
- (2), (5–6) None.

## Notes

The code points of a text are walked without pieces by [runes](../runes/README.md), as `char32_t`s.

The pieces are kept as they are by a container of slices, `vector<string_slice> parts(s.split(','))`, or as strings
of their own by a container of strings, `vector<string> parts(s.split(','))`: every sequence of the library has a
constructor from a range.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string csv = "a,b,,c,";
    for (string_slice piece : csv.split(',')) {
        print("[{}]", piece);
    }
    println("");

    for (string_slice piece : csv.split(',', 2)) {
        print("[{}]", piece);
    }
    println("");

    vector<string> parts(string("usr::local::bin").split("::"));
    println("{} {}", parts.size(), parts[2]);

    vector<string> dots(string("a·b·c").split(U'·'));
    vector<string> letters(string("żółw").split(""));  // a code point a piece
    println("{} {} {}", dots.size(), letters.size(), letters[1]);

    println("{} {}", string().split(',').empty(), csv.split(';').empty());
}
```

Output:

```text
[a][b][][c][]
[a][b,,c,]
3 bin
3 4 ó
true false
```

## See also

- [fields](fields.md): the words between runs of white space
- [join](join.md): one string of the pieces with a separator between each two
- [pieces](../string-pieces/README.md): the range `split` returns
- [slice](../slice/README.md): what a piece is
- [sgcl::string](README.md)
