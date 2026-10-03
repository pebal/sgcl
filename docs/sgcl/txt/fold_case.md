[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::fold_case

```cpp
#include "sgcl/txt/case.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    string fold_case(const string& text);
}
```

Returns `text` folded for comparison by Unicode's full case folding: not a case of its own and not for showing,
only for asking whether two texts are the same word but for their case. `"straße"` and `"STRASSE"` fold alike; a
final sigma folds to `σ` like any other, and no language is asked. Two texts are equal without regard to case when
their foldings are equal ([equal_fold_full](equal_fold_full.md)).

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text, UTF-8 |

## Return value

The folded text.

## Complexity

Linear in the length of the text. A text all ASCII is folded byte by byte.

## Exceptions

`length_error` when the result would pass a string's `max_size()`.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    println("{} | {}", txt::fold_case("STRASSE"), txt::fold_case("die Straße"));
    println("{}", txt::fold_case("ΣΊΣΥΦΟΣ ﬁ"));
}
```

Output:

```text
strasse | die strasse
σίσυφοσ fi
```

## See also

- [equal_fold_full](equal_fold_full.md)
- [find_fold](find_fold.md): a search that folds both sides
- [nfkc_casefold](nfkc_casefold.md): the folding of identifiers
- [sgcl::txt](README.md)
