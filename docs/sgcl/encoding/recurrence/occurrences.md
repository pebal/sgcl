[sgcl](../../README.md) › [encoding](../README.md) › [recurrence](README.md)

# sgcl::encoding::recurrence::occurrences

```cpp
vector<time::datetime> occurrences(const time::datetime& start, const time::datetime& from, const time::datetime& to) const;                  // (1)
vector<time::datetime> occurrences(const time::datetime& start, const time::datetime& from, const time::datetime& to, size_t limit) const;    // (2)
```

The instances from the start in its zone, by the [rules](README.md#rules): the start first, matching the rule or
not (the first instance, as DTSTART is), then the rule's times after it; those at or after `from` and before `to`,
in order.

1. At most 100 000.
2. At most `limit`.

The periods before `from` are passed over when the rule has no COUNT; a rule that makes no time in ten million
periods in a row (`FREQ=SECONDLY;BYSETPOS=2`) ends there.

## Parameters

| Parameter | Description |
|---|---|
| `start` | the first instance: its clock and its zone are the rule's |
| `from`, `to` | the range, `to` not in it |
| `limit` | the most instances |

## Return value

The instances.

## Complexity

Linear in the periods from the start (from `from` without COUNT) to `to`.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/time.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto ny = time::zone::load("America/New_York").value();
    auto start = time::date(2026, 3, 1).at(9, 0, ny);
    encoding::recurrence weekly("FREQ=WEEKLY;BYDAY=SU,TU");
    for (const auto& t : weekly.occurrences(start, start, time::date(2026, 3, 12).at(0, 0, ny))) {
        println(t.to_string());
    }
}
```

Output:

```text
2026-03-01T09:00:00-05:00
2026-03-03T09:00:00-05:00
2026-03-08T09:00:00-04:00
2026-03-10T09:00:00-04:00
```

## See also

- [icalendar::occurrences](../icalendar/occurrences.md)
- [sgcl::encoding::recurrence](README.md)
