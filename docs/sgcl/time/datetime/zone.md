[sgcl](../../README.md) › [time](../README.md) › [datetime](../datetime.md)

# sgcl::time::datetime::zone

```cpp
time::zone zone() const noexcept;
```

The [zone](../zone.md) the instant is seen in, Go's `t.Location()`. Two datetimes compare equal by their instants
alone; `a.zone() == b.zone()` asks whether they are seen in the same zone too.

## Parameters

None.

## Return value

The zone; UTC for a default-constructed datetime.

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
    auto t = time::date(2026, 9, 24).at(12, 41, time::zone("Europe/Warsaw"));
    println("{} {}", t.zone().name(), time::datetime().zone().name());
    println("{} {}", t == t.utc(), t.zone() == t.utc().zone());
}
```

Output:

```text
Europe/Warsaw UTC
true false
```

## See also

- [in](in.md): the same instant in another zone
- [offset](offset.md), [abbreviation](abbreviation.md), [is_dst](is_dst.md): what the zone is at the instant
- [sgcl::time::datetime](../datetime.md)
