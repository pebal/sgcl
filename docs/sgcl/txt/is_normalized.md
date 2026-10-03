[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::is_normalized

```cpp
#include "sgcl/txt/normalize.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    template<class Form>
    bool is_normalized(const string& text, Form form);
}
```

Checks whether the text is in the normalization form already ([the forms](nfc_t.md)). The quick check properties
of the UCD answer most texts without looking at a single decomposition; where they say "maybe", the text is
normalized and compared with itself. [normalize](normalize.md) asks the same quick check itself, so a text is not
checked first and normalized after.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text, UTF-8 |
| `form` | `txt::nfc`, `txt::nfd`, `txt::nfkc` or `txt::nfkd` |

## Return value

`true` when `normalize(text, form) == text`; `false` for a text with an invalid byte of UTF-8, which is in no
form.

## Complexity

Linear in the length of the text.

## Exceptions

`length_error` when the normalized text, made where the quick check says "maybe", would pass the
[max_size()](../core/string/max_size.md) of a string.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    string one = "\u00e9";    // é, one code point
    string two = "e\u0301";   // e and a combining acute
    println("{} {}", txt::is_normalized(one, txt::nfc), txt::is_normalized(one, txt::nfd));
    println("{} {}", txt::is_normalized(two, txt::nfc), txt::is_normalized(two, txt::nfd));
    println("{}", txt::is_normalized("ﬁ", txt::nfkc));
}
```

Output:

```text
true false
false true
false
```

## See also

- [normalize](normalize.md): the text in a form
- [is_nfkc_casefolded](is_nfkc_casefolded.md): whether a name is in the form names are compared in
- [txt](README.md)
