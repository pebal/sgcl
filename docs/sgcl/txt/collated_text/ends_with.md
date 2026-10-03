[sgcl](../../README.md) › [txt](../README.md) › [collated_text](../collated_text.md)

# sgcl::txt::collated_text::ends_with

```cpp
/*(1)*/ bool ends_with(const searcher_type& pattern) const noexcept;
/*(2)*/ bool ends_with(const string& pattern) const noexcept;
```

Checks whether the text ends with a pattern, by the same equality and the same boundaries as [find](find.md), with
nothing behind the match that this collator looks at. Such a match takes the last elements the collator looks at
and no others — one fewer and something it looks at would be left behind it, one more and the pattern would not be
as long as it is — so there is a single element it can begin at, and finding it is a walk back over the pattern's
length rather than over the text's.

1. A pattern weighed once; one of another collator is weighed again with the text's own.
2. A pattern as text, weighed on the call.

## Parameters

| Parameter | Description |
|---|---|
| `pattern` | the ending to look for |

## Return value

`true` when the text ends with the pattern, `false` otherwise; `true` for an empty pattern, `false` for one of
which the collator looks at nothing.

## Complexity

Linear in the length of the pattern, and in what stands behind the match that the collator does not look at
([Benchmarks: collation](../benchmarks.md#collation)). (2) weighs the pattern first.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::collated_text name(txt::collator(txt::strength::primary), "Zażółć gęślą JAŹŃ");
    println("{} {}", name.ends_with("jazn"), name.ends_with("gesla"));
}
```

Output:

```text
true false
```

## See also

- [starts_with](starts_with.md): the other end
- [sgcl::txt::collated_text](../collated_text.md)
