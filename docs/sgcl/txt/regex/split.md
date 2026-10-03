[sgcl](../../README.md) › [txt](../README.md) › [regex](../regex.md)

# sgcl::txt::regex::split

```cpp
/*(1)*/ vector<slice<const char>> split(const slice<const char>& text,
                                        size_t limit = 0) const noexcept;
/*(2)*/ vector<slice<const char>> split(const string& text, size_t limit = 0) const noexcept;
/*(3)*/ template<size_t N>
        vector<slice<const char>> split(const char (&text)[N], size_t limit = 0) const;
/*(4)*/ template<class P> requires std::same_as<P, const char*> || std::same_as<P, char*>
        vector<slice<const char>> split(P text, size_t limit = 0) const;
```

Returns the pieces of the text between the matches of the pattern, as slices of it: Python's `re.split` without the
groups, Go's `Split`. A text with no match is one piece, the whole of it; a match at an edge leaves an empty piece
there. A match of no width splits too, and the search moves on by one code point, which stays in the next piece.

1. The text as a slice; the pieces are slices of it.
2. The text as a string; the pieces are slices of it.
3. An array of `char` up to its first NUL or its end, copied into a string the pieces hold.
4. The characters at a pointer up to their NUL, copied into a string the pieces hold.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text, UTF-8 |
| `limit` | the most pieces to give, the last of them the whole rest of the text; `0` is no limit |

## Return value

The pieces, in the order of the text; at least one.

## Complexity

Linear in the length of the text times the length of the pattern.

## Exceptions

- (1–2) None.
- (3–4) `length_error` when the text is longer than 4294967295 bytes, the most a [string](../../core/string.md)
  holds.

## Notes

Each piece is a [slice](../../core/slice.md) that holds the text's object: the pieces outlive the string they were
cut from, with no copy of their bytes.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::regex comma("\\s*,\\s*");
    println("{}", comma.split("jabłka , gruszki,śliwki ,"));
    println("{}", comma.split("a, b, c, d", 2));
    println("{}", txt::regex("x*").split("aż"));
}
```

Output:

```text
["jabłka", "gruszki", "śliwki", ""]
["a", "b, c, d"]
["", "a", "ż", ""]
```

## See also

- [replace](replace.md): the matches replaced
- [all](all.md): the matches themselves
- [sgcl::txt::regex](../regex.md)
