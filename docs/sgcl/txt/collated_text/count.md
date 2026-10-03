[sgcl](../../README.md) › [txt](../README.md) › [collated_text](README.md)

# sgcl::txt::collated_text::count

```cpp
size_t count(const searcher_type& pattern) const noexcept;    // (1)
size_t count(const string& pattern) const noexcept;           // (2)
```

Counts the occurrences of a pattern in the weighed text that do not overlap, left to right: the next is looked for
past the end of the last. An empty pattern counts none.

1. A pattern weighed once; one of another collator is weighed again with the text's own.
2. A pattern as text, weighed on the call.

## Parameters

| Parameter | Description |
|---|---|
| `pattern` | the pattern to look for |

## Return value

The number of occurrences that do not overlap.

## Complexity

A scan linear in the text on ordinary text and the text times the pattern at worst; (2) weighs the pattern first.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::collator danish(txt::locale("da"), txt::strength::primary);
    txt::collated_text text(danish, "Aarhus, Ålborg, Aalborg");
    println("{} {}", text.count("å"), text.count("a"));
}
```

Output:

```text
3 0
```

## See also

- [find](find.md): the first occurrence
- [collated_matches](../collated_matches/README.md): every occurrence, as a range
- [sgcl::txt::collated_text](README.md)
