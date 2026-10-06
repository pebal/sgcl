[sgcl](../../README.md) › [encoding](../README.md) › [icalendar](README.md)

# sgcl::encoding::operator== (sgcl::encoding::icalendar)

```cpp
friend bool operator==(const icalendar& a, const icalendar& b) noexcept;
```

Whether the components have the same name, the same properties in the same order (by
[content_line](../content_line/operator_cmp.md)'s equality) and equal components inside in the same order; `!=` is made
from it by the compiler.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the components |

## Return value

`true` when they are equal.

## Complexity

Linear in the size of the components.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::icalendar a;
    println("{} {}", a == encoding::icalendar::parse(a.to_string()).value(), a == a.erase("PRODID"));
}
```

Output:

```text
true false
```

## See also

- [sgcl::encoding::icalendar](README.md)
