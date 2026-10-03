[sgcl](../../README.md) › [txt](../README.md) › [regex](../regex.md)

# sgcl::txt::regex::contains

```cpp
/*(1)*/ bool contains(const slice<const char>& text) const noexcept;
/*(2)*/ bool contains(const string& text) const noexcept;
/*(3)*/ template<size_t N> bool contains(const char (&text)[N]) const noexcept;
/*(4)*/ template<class P> requires std::same_as<P, const char*> || std::same_as<P, char*>
        bool contains(P text) const noexcept;
```

Checks whether the pattern matches somewhere in the text: Go's `MatchString`, `std::regex_search` without the
results. Where the groups and the place are not wanted it is cheaper than [find](find.md): only the edges of the
whole match are followed.

- (1–4) The text as a slice, a string, an array of `char` up to its first NUL or its end, or the characters at a
  pointer up to their NUL.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text, UTF-8 |

## Return value

`true` when the pattern matches some part of the text, the empty part included, `false` otherwise.

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
    txt::regex word("\\bkot\\b");
    println("{} {}", word.contains("Ala ma kota"), word.contains("a kot ma Alę"));
}
```

Output:

```text
false true
```

## See also

- [full_match](full_match.md): whether the whole text matches
- [find](find.md): the first match, with its place and its groups
- [sgcl::txt::regex](../regex.md)
