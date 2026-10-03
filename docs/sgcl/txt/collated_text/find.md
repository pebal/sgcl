[sgcl](../../README.md) › [txt](../README.md) › [collated_text](../collated_text.md)

# sgcl::txt::collated_text::find

```cpp
optional<match> find(const searcher_type& pattern, size_t from = 0) const noexcept;    // (1)
optional<match> find(const string& pattern, size_t from = 0) const noexcept;           // (2)
```

Finds where a pattern is in the weighed text at or after the byte `from`, counting as equal what the collator
counts as equal, by the rules of [collator::find](../collator/find.md). Only the search is paid: the text was
weighed when the object was built.

1. A pattern weighed once. One weighed by another collator than the text's is weighed again here, with the text's
   own, and the answer is the one (2) gives.
2. A pattern as text, weighed on the call.

An empty pattern is found at `from` while `from` is not past the end of the text; a pattern that is not empty but
of which the collator looks at nothing is found nowhere.

## Parameters

| Parameter | Description |
|---|---|
| `pattern` | the pattern to look for |
| `from` | the byte of the text the search starts at |

## Return value

The [match](../collator-match.md) — the byte position and the bytes it covers in the text — or an empty `optional`
when there is none at or after `from`. The size is the text's own: six letters of a pattern may be found in eight
bytes of text, an accent taking bytes the pattern never had.

## Complexity

A bisection to `from`, then a scan linear in the rest of the text on ordinary text and the text times the pattern
at worst; (2), and (1) of another collator, weigh the pattern first.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    string text = "Résumé, resume, RESUME";
    txt::collated_text weighed(txt::collator(txt::strength::primary), text);
    for (auto m = weighed.find("resume"); m; m = weighed.find("resume", m->at + m->size)) {
        print("[{}] ", text.as_slice(m->at, m->size));
    }
    println();
}
```

Output:

```text
[Résumé] [resume] [RESUME] 
```

## See also

- [contains](contains.md), [count](count.md)
- [collated_matches](../collated_matches.md): every occurrence, as a range
- [sgcl::txt::collated_text](../collated_text.md)
