[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::case_order

```cpp
#include "sgcl/txt/collate.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    enum class case_order : uint8_t {
        natural,
        upper_first,
        lower_first,
    };
}
```

Which case comes first to a [collator](collator.md) where the letters and the accents are the same. The root puts
the small letter first; Danish, Maltese and Church Slavonic ask for the capital, and so do the lists a lawyer
reads. CLDR calls the setting `caseFirst` (`kf`); it is the `case_order` of
[collator::options](collator-options.md), and [collator::capitals_first](collator/capitals_first.md) says what a
collator settled on.

| Value | Description |
|---|---|
| `natural` | the order of the weights, which is the small letter first in the root |
| `upper_first` | the capitals before the small letters |
| `lower_first` | the small letters first: what the root does anyway, here so that a caller can say so over a language that asks for the other |

The case of a letter is read from the letter and not from its weights, so the order holds for a letter a language
moved as well: Danish `Œ` and Maltese `Għ` sort where ICU sorts them, and a letter written in both cases — Danish
sorts `Aa` as one letter — falls between the two, as it does there. Kana have no case, and here they take the one
CLDR gives them from the third weight of the root: a small kana (ぁ, ッ, ｧ) is a small letter and a normal one
(あ, ツ, ｱ) a capital, as in ICU.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    vector<string> names = {"ala", "Ala", "ąla", "Ąla"};
    names.sort(txt::collator(txt::locale("pl")));
    println("{}", names);
    names.sort(txt::collator(txt::locale("pl"), {.case_order = txt::case_order::upper_first}));
    println("{}", names);
}
```

Output:

```text
["ala", "Ala", "ąla", "Ąla"]
["Ala", "ala", "Ąla", "ąla"]
```

## See also

- [collator::options](collator-options.md): the settings of a collator
- [collator](collator.md)
