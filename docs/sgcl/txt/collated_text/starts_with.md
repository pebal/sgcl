[sgcl](../../README.md) › [txt](../README.md) › [collated_text](../collated_text.md)

# sgcl::txt::collated_text::starts_with

```cpp
/*(1)*/ bool starts_with(const searcher_type& pattern) const noexcept;
/*(2)*/ bool starts_with(const string& pattern) const noexcept;
```

Checks whether the text begins with a pattern, by the same equality and the same boundaries as [find](find.md),
asked at one end of the text: with nothing in front of the match that this collator looks at. Not at byte zero,
because that is a question about the bytes: with the punctuation shifted a hyphen is not there to be found around,
and not at the ends either, so `"-resume"` starts with `"resume"` as it contains it.

1. A pattern weighed once; one of another collator is weighed again with the text's own.
2. A pattern as text, weighed on the call.

## Parameters

| Parameter | Description |
|---|---|
| `pattern` | the beginning to look for |

## Return value

`true` when the text begins with the pattern, `false` otherwise; `true` for an empty pattern, `false` for one of
which the collator looks at nothing.

## Complexity

The elements up to the first one the collator looks at: constant on ordinary text. (2) weighs the pattern first.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::collator shifted{{.strength = txt::strength::primary,
                           .punctuation = txt::punctuation::shifted}};
    txt::collated_text text(shifted, "-- Résumé --");
    println("{} {}", text.starts_with("resume"), text.ends_with("resume"));
}
```

Output:

```text
true true
```

## See also

- [ends_with](ends_with.md): the other end
- [sgcl::txt::collated_text](../collated_text.md)
