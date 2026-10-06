[sgcl](../../README.md) › [encoding](../README.md) › [asn1](README.md)

# sgcl::encoding::asn1::generalized_time

```cpp
static asn1 generalized_time(const time::datetime& t) noexcept;
```

A GeneralizedTime of the instant: `YYYYMMDDHHMMSS[.f]Z` in UTC, the form DER has (X.690 §11.7), the
fraction of a second written when there is one, without its trailing zeros. Go writes no fraction.

## Parameters

| Parameter | Description |
|---|---|
| `t` | the instant, in any zone |

## Return value

The element.

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
    auto t = time::datetime::from_unix_milli(1700000000250, time::zone::utc());
    println(*encoding::asn1::generalized_time(t).as_string());
    auto later = time::datetime::from_unix(4102444800, time::zone::utc());
    println(*encoding::asn1::generalized_time(later).as_string());
}
```

Output:

```text
20231114221320.25Z
21000101000000Z
```

## See also

- [utc_time](utc_time.md): the years 1950 to 2049
- [as_time](as_time.md): the instant read
- [sgcl::encoding::asn1](README.md)
