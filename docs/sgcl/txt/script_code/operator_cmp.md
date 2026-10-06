[sgcl](../../README.md) › [txt](../README.md) › [script_code](README.md)

# sgcl::txt::script_code::operator==

```cpp
constexpr bool operator==(const script_code&) const noexcept = default;
```

Checks whether two codes are one. `!=` is its negation.

## Parameters

| Parameter | Description |
|---|---|
| `(unnamed)` | the other |

## Return value

`true` when both are the same code, or none.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"
#include "sgcl/txt/names/de.h"
#include "sgcl/txt/names/pl.h"

using namespace sgcl;

int main() {
    println("{}", txt::script_code("latn") == txt::script_code("Latn"));
}
```

Output:

```text
true
```

## See also

- [code](code.md)
- [sgcl::txt::script_code](README.md)
