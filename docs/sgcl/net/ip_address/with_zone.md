[sgcl](../../README.md) › [net](../README.md) › [ip_address](../ip_address.md)

# sgcl::net::ip_address::with_zone

```cpp
ip_address with_zone(const string& zone) const;
```

The address with the zone given, Go's `Addr.WithZone`: the empty string removes the zone. An IPv4 address has no zone
and is returned unchanged.

The zone lives in the value, at most fifteen bytes, the most an interface name has (`IFNAMSIZ` with its terminator).
Here this type differs from Go's, whose zone is a pointer to a string of any length: a longer zone is a broken
contract and throws, and so is a zone with a NUL in it (a zone goes to `if_nametoindex` as a C string), which Go
keeps.

## Parameters

| Parameter | Description |
|---|---|
| `zone` | the zone: an interface name or index, at most fifteen bytes without a NUL; empty for none |

## Return value

The address with the zone; `*this` for an IPv4 address and for the empty one.

## Complexity

Linear in the length of `zone`.

## Exceptions

`invalid_argument` when `zone` is longer than fifteen bytes or holds a NUL, and the address is IPv6.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::ip_address a("fe80::1");
    println(a.with_zone("en0").to_string());
    println(a.with_zone("en0").with_zone("").to_string());
    println(net::ip_address("10.0.0.1").with_zone("en0").to_string());

    try {
        a.with_zone("a-very-long-interface");
    } catch (const invalid_argument& e) {
        println(e.what());
    }
}
```

Output:

```text
fe80::1%en0
fe80::1
10.0.0.1
sgcl::net::ip_address::with_zone: a zone is at most 15 bytes, without NUL
```

## See also

- [zone](zone.md): the zone of an address
- [sgcl::net::ip_address](../ip_address.md)
