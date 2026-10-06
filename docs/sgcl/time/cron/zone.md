[sgcl](../../README.md) › [time](../README.md) › [cron](README.md)

# sgcl::time::cron::zone

```cpp
time::zone zone() const noexcept;
```

Returns the zone whose clock the cron's times are read on: the one given to [parse](parse.md) or the constructor, the
local zone by default. The times [next](next.md) gives are datetimes in it.

## Parameters

None.

## Return value

The zone.

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
    time::cron c("@daily", time::zone("America/New_York"));
    println("{}", c.zone().name());
}
```

Output:

```text
America/New_York
```

## See also

- [time::zone](../zone/README.md)
- [sgcl::time::cron](README.md)
