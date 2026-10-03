[sgcl](../README.md) › [txt](README.md) › [collator](collator.md)

# sgcl::txt::collator::match

```cpp
#include "sgcl/txt/collate.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    class collator {
    public:
        struct match {
            size_t at;
            size_t size;
        };
    };
}
```

`sgcl::txt::collator::match` is where a search by collation found its pattern, in the bytes of the text as it was
given: what [collator::find](collator/find.md) and [collated_text::find](collated_text/find.md) return. The size is
the text's own and not the pattern's: six letters of a pattern may be found in eight bytes of text, an accent taking
bytes the pattern never had. It is the [occurrence](occurrence.md) of the folded and normalized searches under the
names `at` and `size`.

## Member objects

| Field | Description |
|---|---|
| `at` | the byte position in the text where the match begins |
| `size` | the bytes of the text the match covers; `0` for an empty pattern |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::collator search{txt::strength::primary};
    string text = "Le résumé du candidat";
    txt::collator::match hit = *search.find(text, "resume");
    println("at {}, size {}: {}", hit.at, hit.size, text.as_slice(hit.at, hit.size));
}
```

Output:

```text
at 3, size 8: résumé
```

## See also

- [collator::find](collator/find.md)
- [collator](collator.md)
