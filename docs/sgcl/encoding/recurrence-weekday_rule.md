[sgcl](../README.md) › [encoding](README.md) › [recurrence](recurrence/README.md)

# sgcl::encoding::recurrence::weekday_rule

```cpp
#include "sgcl/encoding/recurrence.h"   // or "sgcl/encoding.h"

namespace sgcl::encoding {
    class recurrence {
    public:
        struct weekday_rule {
            time::weekday day = time::weekday::monday;
            int ordinal = 0;
        };
    };
}
```

`sgcl::encoding::recurrence::weekday_rule` is a day of a rule's BYDAY, as [by_day](recurrence/by_month.md) gives it:
`MO` is every Monday, `1MO` the first Monday, `-1FR` the last Friday (of the month for MONTHLY and for YEARLY with
BYMONTH, of the year otherwise).

## Rules

- `weekday_rule` is plain data: it lives anywhere; `==` compares both fields.

## Member objects

| Object | Description |
|---|---|
| `day` | the weekday |
| `ordinal` | which of them: 1 to 53 from the start, -1 to -53 from the end; 0 every one |

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/time.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    for (const auto& [day, ordinal] : encoding::recurrence("FREQ=MONTHLY;BYDAY=MO,2TU,-1FR").by_day()) {
        println("{} {}", day, ordinal);
    }
}
```

Output:

```text
Monday 0
Tuesday 2
Friday -1
```

## See also

- [by_day](recurrence/by_month.md)
- [sgcl::encoding::recurrence](recurrence/README.md)
