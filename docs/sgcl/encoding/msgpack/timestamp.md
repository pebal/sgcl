[sgcl](../../README.md) › [encoding](../README.md) › [msgpack](README.md)

# sgcl::encoding::msgpack::timestamp

```cpp
static cbor timestamp(const time::datetime& t) noexcept;
```

The timestamp extension (type -1) of the instant, in the shortest of its three forms: 32 bits of seconds since
1970 when there is no fraction and they fit; 30 bits of nanoseconds and 34 of seconds; or 32 bits of nanoseconds
and 64 of signed seconds (before 1970 among them). [cbor::as_time](../cbor/as_time.md) reads it back.

## Parameters

| Parameter | Description |
|---|---|
| `t` | the instant |

## Return value

The extension.

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
    for (int64_t ns : {int64_t(1700000000000000000), int64_t(1700000000500000000), int64_t(-1000000000)}) {
        encoding::cbor ts = encoding::msgpack::timestamp(time::datetime::from_unix_nano(ns, time::zone::utc()));
        println("{} {}", encoding::hex::encode(encoding::msgpack::encode(ts)), ts.as_time()->to_string());
    }
}
```

Output:

```text
d6ff6553f100 2023-11-14T22:13:20Z
d7ff773594006553f100 2023-11-14T22:13:20.5Z
c70cff00000000ffffffffffffffff 1969-12-31T23:59:59Z
```

## See also

- [cbor::as_time](../cbor/as_time.md)
- [sgcl::encoding::msgpack](README.md)
