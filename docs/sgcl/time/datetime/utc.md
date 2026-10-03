[sgcl](../../README.md) › [time](../README.md) › [datetime](README.md)

# sgcl::time::datetime::utc

```cpp
datetime utc() const noexcept;
```

The same instant seen in UTC, Go's `t.UTC()`: `in(time::zone::utc())`.

## Parameters

None.

## Return value

The datetime of the same instant in UTC.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/time.h"

using namespace sgcl;

int main() {
    auto t = time::date(2026, 9, 24).at(12, 41, 15, time::zone("Europe/Warsaw"));
    println("{} {}", t, t.utc());
}
```

Output:

```text
2026-09-24T12:41:15+02:00 2026-09-24T10:41:15Z
```

## See also

- [in](in.md): in any zone
- [local](local.md): in the local zone
- [sgcl::time::datetime](README.md)
