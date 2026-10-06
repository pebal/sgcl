[sgcl](../../README.md) › [encoding](../README.md) › [asn1](README.md)

# sgcl::encoding::asn1::utc_time

```cpp
static asn1 utc_time(const time::datetime& t);
```

A UTCTime of the instant: `YYMMDDHHMMSSZ` in UTC, the one form DER has (X.690 §11.8). Two digits of
the year stand for 1950 to 2049 (RFC 5280), so an instant outside them has no UTCTime; the part of a second is
dropped. X.509 writes its times as UTCTime up to 2049 and as [generalized_time](generalized_time.md) after.

## Parameters

| Parameter | Description |
|---|---|
| `t` | the instant, in any zone |

## Return value

The element.

## Complexity

Constant.

## Exceptions

`invalid_argument` for an instant before 1950 or after 2049.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"
#include "sgcl/time.h"

using namespace sgcl;

int main() {
    auto t = time::datetime::from_unix(1700000000, time::zone::utc());
    encoding::asn1 e = encoding::asn1::utc_time(t);
    println("{} {}", *e.as_string(), e.as_time()->unix());
}
```

Output:

```text
231114221320Z 1700000000
```

## See also

- [generalized_time](generalized_time.md): any year
- [as_time](as_time.md): the instant read
- [sgcl::encoding::asn1](README.md)
