[sgcl](../../README.md) › [txt](../README.md) › [fold_searcher](README.md)

# sgcl::txt::fold_searcher::count

```cpp
size_t count(const string& text) const noexcept;
```

Counts the occurrences of the prepared pattern in the text that do not overlap, left to right: the next is looked
for past the end of the last, as [searcher::count](../searcher/count.md) counts them. The text is mapped once for
the whole count. An empty pattern counts none.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text to search, UTF-8 |

## Return value

The number of occurrences that do not overlap.

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
    txt::fold_searcher ss("ss");
    println("{}", ss.count("Straße, STRASSE, strasse"));
}
```

Output:

```text
3
```

## See also

- [find](find.md): the first occurrence
- [fold_matches, normalized_matches](../fold_matches/README.md): every occurrence, as a range
- [sgcl::txt::fold_searcher, normalized_searcher](README.md)
