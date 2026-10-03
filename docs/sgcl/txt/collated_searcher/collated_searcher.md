[sgcl](../../README.md) › [txt](../README.md) › [collated_searcher](README.md)

# sgcl::txt::collated_searcher::collated_searcher

```cpp
collated_searcher(const collator& by, const string& pattern) noexcept;
```

Keeps a copy of the collator and the pattern, and weighs the pattern: its elements, of which only those the
collator looks at are kept — an accent on its own at primary strength leaves nothing.

## Parameters

| Parameter | Description |
|---|---|
| `by` | the collator whose equality the search counts by |
| `pattern` | the text to look for, UTF-8 |

## Complexity

Linear in the length of the pattern.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::collator primary{txt::strength::primary};
    txt::collator tertiary;
    println("{} {}", txt::collated_searcher(primary, "Résumé").size(),
            txt::collated_searcher(tertiary, "Résumé").size());
}
```

Output:

```text
6 8
```

## See also

- [collated_text::searcher](../collated_text/searcher.md): a searcher of the text's own collator
- [sgcl::txt::collated_searcher](README.md)
