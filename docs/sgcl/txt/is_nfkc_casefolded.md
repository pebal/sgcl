[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::is_nfkc_casefolded

```cpp
#include "sgcl/txt/identifier.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    bool is_nfkc_casefolded(const string& text);
}
```

Checks whether the text is in NFKC_Casefold already ([nfkc_casefold](nfkc_casefold.md)), which is the question asked
of every name that arrives. Two properties answer it without folding anything: the text changes under the mapping if
any of its code points does, and what is left is NFC, which the quick check properties of the normalization answer.
Only a "maybe" from those costs the fold.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text, UTF-8 |

## Return value

`true` when `nfkc_casefold(text) == text`.

## Complexity

Linear in the length of the text.

## Exceptions

`length_error` when the folded text, made where the quick check says "maybe", would pass the
[max_size()](../core/string/max_size.md) of a string.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    for (auto s : {"full", "Full", "ﬁle", "strasse", "straße"}) {
        println("{}: {}", s, txt::is_nfkc_casefolded(s));
    }
}
```

Output:

```text
full: true
Full: false
ﬁle: false
strasse: true
straße: false
```

## See also

- [nfkc_casefold](nfkc_casefold.md): the text folded
- [is_normalized](is_normalized.md): the same question of a normalization form
- [txt](README.md)
