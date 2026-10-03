[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::equal_fold_full

```cpp
#include "sgcl/txt/case.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    bool equal_fold_full(const string& a, const string& b);
}
```

Checks whether `a` and `b` are the same text but for their case: equal, or equal once folded
([fold_case](fold_case.md)). Core's `string::equal_fold` compares one code point to one, which is enough for most
texts and wrong for `ß` against `SS`.

## Parameters

| Parameter | Description |
|---|---|
| `a` | a text, UTF-8 |
| `b` | the other |

## Return value

`true` when the foldings are equal.

## Complexity

Constant when the texts are equal; otherwise linear in their lengths: both are folded.

## Exceptions

`length_error` when the result would pass a string's `max_size()`.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    string german = "die Straße";
    println("{} {}", txt::equal_fold_full(german, "DIE STRASSE"), german.equal_fold("DIE STRASSE"));
}
```

Output:

```text
true false
```

## See also

- [fold_case](fold_case.md)
- [equal_fold](../core/mixin/text/equal_fold.md): one code point to one
- [sgcl::txt](README.md)
