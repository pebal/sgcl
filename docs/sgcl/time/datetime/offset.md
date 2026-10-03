[sgcl](../../README.md) › [time](../README.md) › [datetime](../datetime.md)

# sgcl::time::datetime::offset

```cpp
duration offset() const noexcept;
```

The offset of the zone's clock from UTC at the instant, the second half of Go's `t.Zone()`: two hours in Warsaw in
summer, one in winter, zero in UTC. An offset of the local mean times of the 19th century keeps its seconds here,
though the text of the datetime writes it to the minute.

## Parameters

None.

## Return value

The offset, a [duration](../../core/duration.md), negative west of Greenwich.

## Complexity

Logarithmic in the number of the zone's changes; constant in UTC and in a fixed zone.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/time.h"

using namespace sgcl;

int main() {
    time::zone warsaw("Europe/Warsaw");
    println("{} {}", time::date(2026, 7, 1).at(12, 0, warsaw).offset(),
            time::date(2026, 12, 1).at(12, 0, warsaw).offset());
    println(time::date(2026, 7, 1).at(12, 0, time::zone("America/New_York")).offset());
    println(time::datetime::from_unix(-2500000000, time::zone("Europe/Brussels")).offset());
}
```

Output:

```text
2h0m0s 1h0m0s
-4h0m0s
17m30s
```

## See also

- [abbreviation](abbreviation.md), [is_dst](is_dst.md): the rest of what the zone is at the instant
- [zone::offset_at](../zone/offset_at.md): the same asked of a zone
- [sgcl::time::datetime](../datetime.md)
