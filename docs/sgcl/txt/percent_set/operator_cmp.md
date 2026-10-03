[sgcl](../../README.md) › [txt](../README.md) › [percent_set](../percent_set.md)

# sgcl::txt::percent_set::operator==

```cpp
constexpr bool operator==(const percent_set&) const noexcept = default;
```

Checks whether two sets hold the same characters, however they were built. `!=` is its negation.

## Parameters

| Parameter | Description |
|---|---|
| `(unnamed)` | the other set |

## Return value

`true` when the sets are equal.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    println("{}", txt::percent_set{"ab"} == txt::percent_set{"ba"});
    println("{}", txt::percent::fragment == txt::percent::query);
}
```

Output:

```text
true
true
```

## See also

- [sgcl::txt::percent_set](../percent_set.md)
