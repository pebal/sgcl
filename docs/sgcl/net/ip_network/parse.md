[sgcl](../../README.md) › [net](../README.md) › [ip_network](README.md)

# sgcl::net::ip_network::parse

```cpp
static expected<ip_network, io::error> parse(const string& text) noexcept;
```

Reads a network in CIDR notation, `address/bits`, as Go's `netip.ParsePrefix` reads one: `"10.0.0.0/8"`,
`"2001:db8::/32"`. The address is read as [ip_address::parse](../ip_address/parse.md) reads one, but without a zone;
the length in decimal, without a sign or a leading zero, 0 to 32 for IPv4 and 0 to 128 for IPv6. The address is kept
as written: `"10.1.2.3/8"` keeps `10.1.2.3`.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text to read |

## Return value

The network, or an [io::error](../../io/error/README.md) of the code `net::errc::invalid_address` ([errc](../errc.md)), the
operation `parse IP network` and the text.

## Complexity

Linear in the length of `text`.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    for (const char* text : {"10.0.0.0/8", "10.1.2.3/8", "2001:DB8::/32", "10.0.0.0/33", "10.0.0.0/08",
                             "fe80::%en0/64", "10.0.0.0"}) {
        auto n = net::ip_network::parse(text);
        println("{} -> {}", text, n ? n->to_string() : n.error().message());
    }
}
```

Output:

```text
10.0.0.0/8 -> 10.0.0.0/8
10.1.2.3/8 -> 10.1.2.3/8
2001:DB8::/32 -> 2001:db8::/32
10.0.0.0/33 -> parse IP network 10.0.0.0/33: invalid address
10.0.0.0/08 -> parse IP network 10.0.0.0/08: invalid address
fe80::%en0/64 -> parse IP network fe80::%en0/64: invalid address
10.0.0.0 -> parse IP network 10.0.0.0: invalid address
```

## See also

- [(constructor)](ip_network.md): the network a literal spells
- [to_string](to_string.md): the text back
- [sgcl::net::ip_network](README.md)
