[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::hash_normalized

```cpp
#include "sgcl/txt/normalize.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    size_t hash_normalized(const string& text) noexcept;
}
```

Returns a hash two texts share when [equal_normalized](equal_normalized.md) says they are one: FNV-1a over the code
points of the text's canonical decomposition, NFD. The key of a map that must not care which form a name arrived
in.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text, UTF-8 |

## Return value

The hash of the text's canonical decomposition.

## Complexity

Linear in the length of the text.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    string typed = "Zaz\u0307o\u0301\u0142c\u0301 ge\u0328s\u0301la\u0328 jaz\u0301n\u0301";
    string pasted = "Zażółć gęślą jaźń";
    map<size_t, string> names;
    names[txt::hash_normalized(typed)] = pasted;
    println("found: {}", names[txt::hash_normalized(pasted)]);
}
```

Output:

```text
found: Zażółć gęślą jaźń
```

## See also

- [equal_normalized](equal_normalized.md): the equality this hash agrees with
- [txt](README.md)
