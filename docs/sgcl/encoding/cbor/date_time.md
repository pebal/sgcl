[sgcl](../../README.md) › [encoding](../README.md) › [cbor](README.md)

# sgcl::encoding::cbor::date_time

```cpp
static cbor date_time(const time::datetime& t) noexcept;
```

Tag 0 over the instant's text in RFC 3339, in UTC, a fraction of a second when there is one (§3.4.1).

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
    auto t = time::datetime::from_unix(1363896240, time::zone::utc());
    encoding::cbor c = encoding::cbor::date_time(t);
    println("{} {}", c.to_string(), encoding::hex::encode(c.to_bytes()));
}
```

Output:

```text
0("2013-03-21T20:04:00Z") c074323031332d30332d32315432303a30343a30305a
```

## See also

- [epoch_time](epoch_time.md)
- [as_time](as_time.md)
- [sgcl::encoding::cbor](README.md)
