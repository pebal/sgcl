[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::compare_normalized

```cpp
#include "sgcl/txt/normalize.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    int compare_normalized(const string& a, const string& b) noexcept;
}
```

Compares two texts by the code points of their canonical decompositions, NFD: an order blind to the way they were
written, zero exactly where [equal_normalized](equal_normalized.md) is `true`. It is the order of the code points,
not a language's: `"Z"` comes before `"a"` and `"ą"` after `"z"`; the order a reader expects is a
[collator](collator/README.md)'s.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the texts, UTF-8 |

## Return value

A negative number when `a` comes first, zero when the two are the same text, a positive number when `b` comes first:
`-1`, `0` or `1`.

## Complexity

Linear in the lengths of the two texts.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    println("{}", txt::compare_normalized("\u00e9", "e\u0301"));
    println("{} {}", txt::compare_normalized("Z", "a"), txt::compare_normalized("ą", "z"));
}
```

Output:

```text
0
-1 -1
```

## See also

- [equal_normalized](equal_normalized.md): the equality this order agrees with
- [collator](collator/README.md): the order of a language
- [txt](README.md)
