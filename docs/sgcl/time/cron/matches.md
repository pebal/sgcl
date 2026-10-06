[sgcl](../../README.md) › [time](../README.md) › [cron](README.md)

# sgcl::time::cron::matches

```cpp
bool matches(const datetime& t) const noexcept;
```

Checks whether `t`, to the second, is one of the cron's times: the instant [next](next.md) gives from the second
before it. So the instant of a jump over a skipped fixed time matches, and the second pass of a repeated one does
not.

## Parameters

| Parameter | Description |
|---|---|
| `t` | the instant, in any zone |

## Return value

`true` when `t`'s second is one of the times.

## Complexity

A search from the second before `t`.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/time.h"

using namespace sgcl;

int main() {
    time::cron noon("0 12 * * *", time::zone::utc());
    println("{}", noon.matches(time::datetime("2026-10-06T12:00:00.5Z", time::rfc3339)));
    println("{}", noon.matches(time::datetime("2026-10-06T12:01:00Z", time::rfc3339)));
}
```

Output:

```text
true
false
```

## See also

- [next](next.md): the times themselves
- [sgcl::time::cron](README.md)
