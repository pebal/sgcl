[sgcl](../../README.md) › [txt](../README.md) › [collated_text](README.md)

# sgcl::txt::collated_text::searcher

```cpp
searcher_type searcher(const string& pattern) const noexcept;
```

Returns a pattern weighed by this text's collator, which is the only kind the text can be asked about without
weighing it again: a [collated_searcher](../collated_searcher/README.md) of the same collator.

## Parameters

| Parameter | Description |
|---|---|
| `pattern` | the text to look for, UTF-8 |

## Return value

The searcher.

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
    txt::collated_text text(txt::collator(txt::strength::primary), "Kraków, KRAKOW, krakow");
    auto krakow = text.searcher("krakow");
    println("{} {}", text.count(krakow), text.find(krakow, 1)->at);
}
```

Output:

```text
3 9
```

## See also

- [collated_searcher](../collated_searcher/README.md)
- [sgcl::txt::collated_text](README.md)
