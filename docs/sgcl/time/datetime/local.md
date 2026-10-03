[sgcl](../../README.md) › [time](../README.md) › [datetime](../datetime.md)

# sgcl::time::datetime::local

```cpp
datetime local() const noexcept;
```

The same instant seen in the local zone, Go's `t.Local()`: `in(time::zone::local())`, the zone the system is set
to.

## Parameters

None.

## Return value

The datetime of the same instant in the local zone.

## Complexity

Constant, but for the first use of the local zone in the program, which settles it once from the system.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/time.h"

using namespace sgcl;

int main() {
    auto t = time::datetime::from_unix(1790246475, time::zone::utc());
    println("{} {}", t, t.local());
}
```

Sample output:

```text
2026-09-24T10:41:15Z 2026-09-24T12:41:15+02:00
```

## See also

- [in](in.md): in any zone
- [utc](utc.md): in UTC
- [zone::local](../zone/local.md): the local zone
- [sgcl::time::datetime](../datetime.md)
