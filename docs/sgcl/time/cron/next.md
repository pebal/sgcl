[sgcl](../../README.md) › [time](../README.md) › [cron](README.md)

# sgcl::time::cron::next

```cpp
optional<datetime> next(const datetime& after) const noexcept;       // (1)
vector<datetime> next(const datetime& after, size_t count) const;    // (2)
```

1. The first of the cron's times strictly after `after`, in the cron's zone; nothing when there is none before the end
   of a datetime's range (an expression that never matches, `0 0 30 2 *`).
2. The next `count` times after `after`, each the next after the one before; fewer when the times end.

The times are those the zone's clock shows, by ISC cron's rule at a change of the clock
([README: Rules](README.md#rules)): a fixed time the clock skips is the instant of the jump, one it shows twice its
first instant; a wildcard's times are every instant that matches. To the second: `after` with a part of a second is
the second it is in.

## Parameters

| Parameter | Description |
|---|---|
| `after` | the instant to search from, in any zone |
| `count` | how many times |

## Return value

1. The time, or nothing.
2. The times, at most `count`.

## Complexity

A search of the calendar from `after`, a few steps a day of the allowed months, and a look at each change of the zone
on the way.

## Exceptions

- (1) None.
- (2) None, but out of memory.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/time.h"

using namespace sgcl;

int main() {
    time::cron rounds("*/30 * * * *", time::zone("Europe/Warsaw"));
    time::datetime from("2026-10-25T01:50:00+02:00", time::rfc3339);
    for (auto& t : rounds.next(from, 5)) {
        println("{}", t.format("%R %Z"));  // the repeated hour twice
    }
    time::cron never("0 0 30 2 *");
    println("{}", never.next(from).has_value());
}
```

Output:

```text
02:00 CEST
02:30 CEST
02:00 CET
02:30 CET
03:00 CET
false
```

## See also

- [matches](matches.md): whether an instant is one of the times
- [every](../every.md): a function called at the times
- [sgcl::time::cron](README.md)
