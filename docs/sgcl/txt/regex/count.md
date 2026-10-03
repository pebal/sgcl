[sgcl](../../README.md) › [txt](../README.md) › [regex](../regex.md)

# sgcl::txt::regex::count

```cpp
size_t count(const slice<const char>& text) const noexcept;                          // (1)
size_t count(const string& text) const noexcept;                                     // (2)
template<size_t N> size_t count(const char (&text)[N]) const noexcept;               // (3)
template<class P> requires std::same_as<P, const char*> || std::same_as<P, char*>
size_t count(P text) const noexcept;                                                 // (4)
```

Counts the matches of the pattern in the text, never overlapping: the matches [all](all.md) walks, walked and
counted, not stored. A match of no width counts, and moves the search on by one code point.

- (1–4) The text as a slice, a string, an array of `char` up to its first NUL or its end, or the characters at a
  pointer up to their NUL.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text, UTF-8 |

## Return value

The number of matches.

## Complexity

Linear in the length of the text times the length of the pattern.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    println("{}", txt::regex("\\bkot\\b").count("kot, kotek i kot"));
    println("{}", txt::regex("x*").count("abc"));  // the four empty places
}
```

Output:

```text
2
4
```

## See also

- [all](all.md): every match, as a range
- [sgcl::txt::regex](../regex.md)
