[sgcl](../../README.md) › [txt](../README.md) › [folded_text](README.md)

# sgcl::txt::folded_text::contains

```cpp
bool contains(const searcher_type& pattern) const noexcept;    // (1)
bool contains(const string& pattern) const noexcept;           // (2)
```

Checks whether a pattern occurs in the mapped text, `find(pattern) != npos`.

1. A pattern mapped once, a [fold_searcher](../fold_searcher/README.md) for a `folded_text` and a
   [normalized_searcher](../fold_searcher/README.md) for a `normalized_text`.
2. A pattern as text, mapped on the call.

## Parameters

| Parameter | Description |
|---|---|
| `pattern` | the pattern to look for |

## Return value

`true` when the pattern occurs in the text, `false` otherwise; `true` for an empty pattern.

## Complexity

A scan linear in the text on ordinary text and the text times the pattern at worst; (2) maps the pattern first,
linear in its length.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::normalized_text names("Zoe\u0308, Chloé, Noël");
    for (string who : {"Zoë", "Chloe\u0301", "Noel"}) {
        print("{} ", names.contains(who));
    }
    println();
}
```

Output:

```text
true true false 
```

## See also

- [find](find.md): where it occurs
- [sgcl::txt::folded_text, normalized_text](README.md)
