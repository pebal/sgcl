[sgcl](../../README.md) › [encoding](../README.md) › [cbor](README.md)

# sgcl::encoding::cbor::as_time

```cpp
optional<time::datetime> as_time() const noexcept;
```

The instant of tag 0 (RFC 3339 text, in the zone of its offset) or tag 1 (seconds since 1970, an integer or a
float, in UTC), as a [datetime](../../time/datetime/README.md); `nullopt` for every other value and for a tag 0 or 1
whose content is not one. An instant past a datetime's years is the end of them.

## Parameters

None.

## Return value

The value, or `nullopt`.

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
    vector<byte> bytes = encoding::hex::decode("c074323031332d30332d32315432303a30343a30305a").value();
    println(encoding::cbor::parse(bytes)->as_time()->to_string());
    println(encoding::cbor::tagged(1, 1363896240.5).as_time()->to_string());
    println(encoding::cbor::tagged(0, "never").as_time().has_value());
}
```

Output:

```text
2013-03-21T20:04:00Z
2013-03-21T20:04:00.5Z
false
```

## See also

- [date_time](date_time.md), [epoch_time](epoch_time.md)
- [sgcl::encoding::cbor](README.md)
