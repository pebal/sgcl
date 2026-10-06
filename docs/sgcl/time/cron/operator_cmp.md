[sgcl](../../README.md) › [time](../README.md) › [cron](README.md)

# sgcl::time::operator==, operator!= (sgcl::time::cron)

```cpp
friend bool operator==(const cron& a, const cron& b) noexcept;
```

Checks whether two crons name the same times in the same zone, however written: `@daily` and `0 0 * * *`, `0 0 * * 7`
and `0 0 * * SUN` are equal. The `!=` is the one C++ writes from this `==`.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the crons to compare |

## Return value

`true` when the fields read are the same and so are the zones.

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
    auto utc = time::zone::utc();
    println("{}", time::cron("@daily", utc) == time::cron("0 0 * * *", utc));
    println("{}", time::cron("@daily", utc) == time::cron("@daily", time::zone("Europe/Warsaw")));
}
```

Output:

```text
true
false
```

## See also

- [to_string](to_string.md): the text as given
- [sgcl::time::cron](README.md)
