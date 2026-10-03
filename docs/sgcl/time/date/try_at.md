[sgcl](../../README.md) › [time](../README.md) › [date](README.md)

# sgcl::time::date::try_at

```cpp
optional<datetime> try_at(int hour, int minute, int second, const zone& z) const noexcept;
```

The instant at which the clock of zone `z` shows this date and that time, as [at](at.md) reads it, when the clock
shows it exactly once; nothing when a change of the clock skipped the time or showed it twice. Hours, minutes and
seconds out of their ranges carry, as in `at`.

## Parameters

| Parameter | Description |
|---|---|
| `hour` | the hour of the clock, 0 to 23; carried when outside |
| `minute` | the minute, 0 to 59; carried when outside |
| `second` | the second, 0 to 59; carried when outside |
| `z` | the zone whose clock shows the time |

## Return value

The [datetime](../datetime/README.md) of that instant, in the zone `z`, or `nullopt`.

## Complexity

Logarithmic in the number of the zone's changes of the clock; constant for a fixed offset.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/time.h"

using namespace sgcl;

int main() {
    time::zone warsaw("Europe/Warsaw");
    for (const char* text : {"2026-03-29", "2026-10-25", "2026-09-01"}) {
        time::date d(text);
        auto t = d.try_at(2, 30, 0, warsaw);
        if (t) {
            println("{}", *t);
        } else {
            println("{} 02:30: skipped or shown twice", d);
        }
    }
}
```

Output:

```text
2026-03-29 02:30: skipped or shown twice
2026-10-25 02:30: skipped or shown twice
2026-09-01T02:30:00+02:00
```

## See also

- [at](at.md): the instant by a rule for every time
- [sgcl::time::date](README.md)
