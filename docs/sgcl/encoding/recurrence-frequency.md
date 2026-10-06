[sgcl](../README.md) › [encoding](README.md) › [recurrence](recurrence/README.md)

# sgcl::encoding::recurrence::frequency

```cpp
#include "sgcl/encoding/recurrence.h"   // or "sgcl/encoding.h"

namespace sgcl::encoding {
    class recurrence {
    public:
        enum class frequency : uint8_t {
            secondly,
            minutely,
            hourly,
            daily,
            weekly,
            monthly,
            yearly
        };
    };
}
```

`sgcl::encoding::recurrence::frequency` is a [recurrence](recurrence/README.md)'s FREQ, as [freq](recurrence/freq.md)
gives it: the length of one period of the rule.

| Value | Description |
|---|---|
| `secondly` | one period is a second |
| `minutely` | one period is a minute |
| `hourly` | one period is an hour |
| `daily` | one period is a day |
| `weekly` | one period is a week, from WKST |
| `monthly` | one period is a month |
| `yearly` | one period is a year (with BYWEEKNO, a week-year) |

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    println(encoding::recurrence("FREQ=HOURLY").freq() == encoding::recurrence::frequency::hourly);
}
```

Output:

```text
true
```

## See also

- [freq](recurrence/freq.md)
- [sgcl::encoding::recurrence](recurrence/README.md)
