[sgcl](../../README.md) › [txt](../README.md) › [regex](../regex.md)

# sgcl::txt::regex::full_match

```cpp
/*(1)*/ bool full_match(const slice<const char>& text) const noexcept;
/*(2)*/ bool full_match(const string& text) const noexcept;
/*(3)*/ template<size_t N> bool full_match(const char (&text)[N]) const noexcept;
/*(4)*/ template<class P> requires std::same_as<P, const char*> || std::same_as<P, char*>
        bool full_match(P text) const noexcept;
```

Checks whether the whole text is a match, its first byte to its last: the question of Python's `re.fullmatch` and
of `std::regex_match`. Go's `MatchString` searches, which is [contains](contains.md) here.

- (1–4) The text as a slice, a string, an array of `char` up to its first NUL or its end, or the characters at a
  pointer up to their NUL.

It is not [find](find.md) with the ends compared afterwards: a search stops at the match a backtracking engine
would have preferred, which may be the shorter one, and the question here is whether the longer one exists at all.
`a|ab` matches the whole of `"ab"`, although `find` gives `a`.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text, UTF-8 |

## Return value

`true` when the pattern matches the whole text, `false` otherwise.

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
    txt::regex code("\\d{2}-\\d{3}");
    println("{} {}", code.full_match("00-950"), code.full_match("kod 00-950"));

    txt::regex either("a|ab");
    println("{} {}", either.full_match("ab"), either.find("ab")->text());
}
```

Output:

```text
true false
true a
```

## See also

- [contains](contains.md): whether the pattern matches somewhere in the text
- [find](find.md): the first match
- [sgcl::txt::regex](../regex.md)
