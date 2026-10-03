[sgcl](../../README.md) › [txt](../README.md) › [collator](../collator.md)

# sgcl::txt::collator::contains

```cpp
bool contains(const string& text, const string& pattern) const noexcept;
```

Checks whether the pattern is found in the text counting as equal what this collator counts as equal: whether
[find](find.md)`(text, pattern)` has a value, with the same boundaries.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text to search, UTF-8 |
| `pattern` | the text to look for |

## Return value

`true` when the pattern is found, `false` otherwise; `true` for an empty pattern, `false` for one the collator does
not look at.

## Complexity

Linear in the length of the text for the weighing; the search is linear in the text on ordinary text and the text
times the pattern at worst.

## Exceptions

None.

## Notes

The text is weighed on every call: a text asked many questions is a [collated_text](../collated_text.md).

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::collator box{{.strength = txt::strength::primary,
                       .punctuation = txt::punctuation::shifted}};
    for (const char* title : {"Résumé writing", "How to re-sume", "Resumption"}) {
        print("{} ", box.contains(title, "resume"));
    }
    println();
}
```

Output:

```text
true true false 
```

## See also

- [find](find.md): where it is found
- [sgcl::txt::collator](../collator.md)
