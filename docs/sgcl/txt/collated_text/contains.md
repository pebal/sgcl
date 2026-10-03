[sgcl](../../README.md) › [txt](../README.md) › [collated_text](../collated_text.md)

# sgcl::txt::collated_text::contains

```cpp
bool contains(const searcher_type& pattern) const noexcept;    // (1)
bool contains(const string& pattern) const noexcept;           // (2)
```

Checks whether a pattern is found in the weighed text, [find](find.md)`(pattern)` has a value.

1. A pattern weighed once; one of another collator is weighed again with the text's own.
2. A pattern as text, weighed on the call.

## Parameters

| Parameter | Description |
|---|---|
| `pattern` | the pattern to look for |

## Return value

`true` when the pattern is found, `false` otherwise; `true` for an empty pattern, `false` for one of which the
collator looks at nothing.

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
    txt::collated_text menu(txt::collator(txt::strength::primary), "Crème brûlée, café, thé");
    for (const char* dish : {"creme brulee", "CAFE", "the", "tea"}) {
        print("{} ", menu.contains(dish));
    }
    println();
}
```

Output:

```text
true true true false 
```

## See also

- [find](find.md): where it is found
- [sgcl::txt::collated_text](../collated_text.md)
