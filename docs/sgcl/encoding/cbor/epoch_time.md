[sgcl](../../README.md) › [encoding](../README.md) › [cbor](README.md)

# sgcl::encoding::cbor::epoch_time

```cpp
static cbor epoch_time(const time::datetime& t) noexcept;
```

Tag 1 over the seconds since 1970 (§3.4.2): an integer, or a float when the instant has a part of a second (a
double holds it to the microsecond).

## Parameters

| Parameter | Description |
|---|---|
| `t` | the instant |

## Return value

The value.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"
#include "sgcl/time.h"

using namespace sgcl;

int main() {
    println(encoding::cbor::epoch_time(time::datetime::from_unix(1363896240, time::zone::utc())).to_string());
    println(encoding::cbor::epoch_time(time::datetime::from_unix_milli(1363896240500, time::zone::utc())).to_string());
}
```

Output:

```text
1(1363896240)
1(1363896240.5)
```

## See also

- [date_time](date_time.md)
- [as_time](as_time.md)
- [sgcl::encoding::cbor](README.md)
