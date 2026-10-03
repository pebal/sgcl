[sgcl](../../README.md) › [txt](../README.md) › [fold_searcher](README.md)

# sgcl::txt::fold_searcher::contains

```cpp
bool contains(const string& text) const noexcept;
```

Checks whether the prepared pattern occurs in the text, [find](find.md)`(text)` has a value: of a `fold_searcher`
without regard to case, of a `normalized_searcher` without regard to the way the text was written.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text to search, UTF-8 |

## Return value

`true` when the pattern occurs in the text, `false` otherwise; `true` for an empty pattern.

## Complexity

Linear in the length of the text for the mapping; the scan is linear in the text on ordinary text and the text
times the pattern at worst.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::normalized_searcher name("Zoë");
    for (string file : {"Zoe\u0308.txt", "zoe.txt", "Zoë (1).txt"}) {
        print("{} ", name.contains(file));
    }
    println();
}
```

Output:

```text
true false true 
```

## See also

- [find](find.md): where it occurs
- [sgcl::txt::fold_searcher, normalized_searcher](README.md)
