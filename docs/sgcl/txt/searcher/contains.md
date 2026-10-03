[sgcl](../../README.md) › [txt](../README.md) › [searcher](README.md)

# sgcl::txt::searcher::contains

```cpp
bool contains(const string& text) const noexcept;                                    // (1)
bool contains(const slice<const char>& text) const noexcept;                         // (2)
template<size_t N> bool contains(const char (&text)[N]) const noexcept;              // (3)
template<class P> requires std::same_as<P, const char*> || std::same_as<P, char*>
bool contains(P text) const noexcept;                                                // (4)
```

Checks whether the pattern occurs in the text, `find(text) != npos`.

- (1–4) The text as a string, a slice, an array of `char` up to its first NUL or its end, or the characters at a
  pointer up to their NUL.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text to search |

## Return value

`true` when the pattern occurs in the text, `false` otherwise; `true` for an empty pattern.

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
    txt::searcher secret("password=");
    for (const char* line : {"user=jan", "password=hunter2", "mode=ro"}) {
        print("{} ", secret.contains(line));
    }
    println();
}
```

Output:

```text
false true false 
```

## See also

- [find](find.md): the position of the first occurrence
- [sgcl::txt::searcher](README.md)
