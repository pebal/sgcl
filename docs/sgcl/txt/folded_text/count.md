[sgcl](../../README.md) › [txt](../README.md) › [folded_text](../folded_text.md)

# sgcl::txt::folded_text::count

```cpp
size_t count(const searcher_type& pattern) const noexcept;    // (1)
size_t count(const string& pattern) const noexcept;           // (2)
```

Counts the occurrences of a pattern in the mapped text that do not overlap, left to right: the next is looked for
past the end of the last, as [searcher::count](../searcher/count.md) counts them. An empty pattern counts none.

1. A pattern mapped once, a [fold_searcher](../fold_searcher.md) for a `folded_text` and a
   [normalized_searcher](../fold_searcher.md) for a `normalized_text`.
2. A pattern as text, mapped on the call.

## Parameters

| Parameter | Description |
|---|---|
| `pattern` | the pattern to look for |

## Return value

The number of occurrences that do not overlap.

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
    txt::normalized_text text("café, cafe\u0301, cafe");
    println("{} {}", text.count("café"), text.count("cafe"));
}
```

Output:

```text
2 1
```

## See also

- [find](find.md): the first occurrence
- [fold_matches, normalized_matches](../fold_matches.md): every occurrence, as a range
- [sgcl::txt::folded_text, normalized_text](../folded_text.md)
