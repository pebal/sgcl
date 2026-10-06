[sgcl](../../README.md) › [encoding](../README.md) › [recurrence](README.md)

# sgcl::encoding::operator== (sgcl::encoding::recurrence)

```cpp
friend bool operator==(const recurrence& a, const recurrence& b) noexcept;
```

Whether the rules have the same parts with the same values in the same order, UNTIL by its text; `!=` is made from
it by the compiler. Two rules of the same instances may differ (`BYMONTH=1,2` and `BYMONTH=2,1`).

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the rules |

## Return value

`true` when they are equal.

## Complexity

Linear in the size of the rules.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    println("{} {}", encoding::recurrence("FREQ=DAILY;INTERVAL=1") == encoding::recurrence(),
            encoding::recurrence("FREQ=DAILY;COUNT=2") == encoding::recurrence());
}
```

Output:

```text
true false
```

## See also

- [sgcl::encoding::recurrence](README.md)
