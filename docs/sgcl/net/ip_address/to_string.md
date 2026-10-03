[sgcl](../../README.md) › [net](../README.md) › [ip_address](README.md)

# sgcl::net::ip_address::to_string

```cpp
string to_string() const noexcept;
```

The text of the address, Go's `Addr.String`. IPv6 is written by RFC 5952: lower case; no leading zeros in a group; the
longest run of two or more zero groups as `::`, the first of equal runs; one zero group written `0`; an IPv4-mapped
address as `::ffff:1.2.3.4`; the zone after `%`. IPv4 is written in dotted decimal, and the empty address as `invalid
IP`, Go's text. The text agrees with Go's byte for byte on the corpus of `tools/ip_oracle.go`.

## Parameters

None.

## Return value

The text, at most 55 bytes (eight groups, seven colons, the `%` and the zone).

## Complexity

Linear in the length of the text.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    for (const char* text : {"2001:0DB8:0:0:1:0:0:1", "2001:db8:0:1:1:1:1:1", "0:0:0:0:0:0:0:0",
                             "::FFFF:10.0.0.1", "FE80::A%en0", "192.168.000.1"}) {
        auto a = net::ip_address::parse(text);
        println("{} -> {}", text, a ? a->to_string() : string("not an address"));
    }
    println(net::ip_address().to_string());
}
```

Output:

```text
2001:0DB8:0:0:1:0:0:1 -> 2001:db8::1:0:0:1
2001:db8:0:1:1:1:1:1 -> 2001:db8:0:1:1:1:1:1
0:0:0:0:0:0:0:0 -> ::
::FFFF:10.0.0.1 -> ::ffff:10.0.0.1
FE80::A%en0 -> fe80::a%en0
192.168.000.1 -> not an address
invalid IP
```

## See also

- [parse](parse.md): the text read
- [sgcl::net::ip_address](README.md)
