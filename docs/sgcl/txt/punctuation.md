[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::punctuation

```cpp
#include "sgcl/txt/collate.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    enum class punctuation : uint8_t {
        counted,
        shifted,
    };
}
```

What a [collator](collator.md) makes of punctuation, spaces and symbols — of what the algorithm calls the variable
elements. CLDR calls the setting `alternate` (`ka`), and Thai asks for `shifted`; it is the `punctuation` of
[collator::options](collator-options.md), and [collator::shifts_punctuation](collator/shifts_punctuation.md) says
what a collator settled on.

| Value | Description |
|---|---|
| `counted` | a hyphen is a character like any other: `re-sume` is as far from `resume` as `rezume` is. What the root order does |
| `shifted` | punctuation, spaces and symbols weigh nothing at the first three levels and only at the fourth, so that `re-sume` and `resume` are one word and a search for `resume` finds `re-sume`: look as though the hyphen were not there. The fourth level is compared only at [strength::quaternary](strength.md), which keeps the two apart while keeping them together |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::collator counted;
    txt::collator shifted{{.punctuation = txt::punctuation::shifted}};
    txt::collator fourth{{.strength = txt::strength::quaternary,
                          .punctuation = txt::punctuation::shifted}};
    println("{} {} {}", counted.equal("re-sume", "resume"), shifted.equal("re-sume", "resume"),
            fourth.equal("re-sume", "resume"));
}
```

Output:

```text
false true false
```

## See also

- [collator::options](collator-options.md): the settings of a collator
- [collator](collator.md)
