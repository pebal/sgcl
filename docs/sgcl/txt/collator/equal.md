[sgcl](../../README.md) › [txt](../README.md) › [collator](README.md)

# sgcl::txt::collator::equal

```cpp
bool equal(const string& a, const string& b) const noexcept;
```

Checks whether two texts are one and the same to this collator, `compare(a, b) == 0`: at primary strength
`resume`, `résumé` and `RESUME` are one word, which is what a duplicate check wants.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the texts, UTF-8 |

## Return value

`true` when the collator puts no difference between the texts, `false` otherwise.

## Complexity

Linear in the lengths of the texts at worst.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::collator search{txt::strength::primary};
    println("resume == résumé == RESUME: {}",
            (search.equal("resume", "résumé") && search.equal("resume", "RESUME")));
}
```

Output:

```text
resume == résumé == RESUME: true
```

## See also

- [compare](compare.md): the order of two texts
- [sgcl::txt::collator](README.md)
