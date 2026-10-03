[sgcl](../../README.md) › [txt](../README.md) › [searcher](README.md)

# sgcl::txt::searcher::count

```cpp
size_t count(const string& text) const noexcept;                                     // (1)
size_t count(const slice<const char>& text) const noexcept;                          // (2)
template<size_t N> size_t count(const char (&text)[N]) const noexcept;               // (3)
template<class P> requires std::same_as<P, const char*> || std::same_as<P, char*>
size_t count(P text) const noexcept;                                                 // (4)
```

Counts the occurrences of the pattern in the text that do not overlap, left to right: the next is looked for past
the end of the last, so `aa` occurs twice in `aaaa`, not three times. An empty pattern counts none.

- (1–4) The text as a string, a slice, an array of `char` up to its first NUL or its end, or the characters at a
  pointer up to their NUL, as [find](find.md) takes it.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text to search |

## Return value

The number of occurrences that do not overlap.

## Complexity

About *n/m* steps on ordinary text, *n* the length of the text and *m* the length of the pattern; *n·m* at worst.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    println("{}", txt::searcher("aa").count("aaaa"));
    println("{}", txt::searcher("kot").count("kot, kotek, kotka"));
    println("{}", txt::searcher("").count("abc"));
    string line = "kot, kotek, kotka";
    println("{}", txt::searcher("kot").count(line.as_slice(4)));  // a piece of a text
}
```

Output:

```text
2
3
0
2
```

## See also

- [find](find.md): the first occurrence
- [sgcl::txt::searcher](README.md)
