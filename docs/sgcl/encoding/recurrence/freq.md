[sgcl](../../README.md) › [encoding](../README.md) › [recurrence](README.md)

# sgcl::encoding::recurrence::freq, interval, count, until

```cpp
frequency freq() const noexcept;             // (1)
int interval() const noexcept;               // (2)
optional<int64_t> count() const noexcept;    // (3)
optional<string> until() const noexcept;     // (4)
```

1. FREQ: the [frequency](../recurrence-frequency.md), the length of one period.
2. INTERVAL: every how many periods; 1 without it.
3. COUNT: how many instances, the start among them; `nullopt` without it.
4. UNTIL as written, a DATE or a DATE-TIME; `nullopt` without it.

## Parameters

None.

## Return value

The part.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::recurrence r("FREQ=WEEKLY;INTERVAL=2;UNTIL=20261231T235959Z");
    println("{} {} {} {}", r.freq() == encoding::recurrence::frequency::weekly, r.interval(), r.count(), r.until());
}
```

Output:

```text
true 2 nullopt "20261231T235959Z"
```

## See also

- [by_month](by_month.md)
- [sgcl::encoding::recurrence](README.md)
