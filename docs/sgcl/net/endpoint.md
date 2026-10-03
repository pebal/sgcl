[sgcl](../README.md) › [net](README.md)

# sgcl::net::endpoint

```cpp
#include "sgcl/net/ip.h"   // or "sgcl/net.h"

namespace sgcl::net {
    class endpoint;
}
```

`sgcl::net::endpoint` is an [ip_address](ip_address.md) and a port, where a socket is bound or connected: Go's
`netip.AddrPort`. Its text is `1.2.3.4:80`, `[::1]:443`, `[fe80::1%en0]:80`, an IPv6 address always in brackets and an
IPv4 address never. It is a value as the address is, 34 bytes, trivially copyable, nothing allocated to parse, copy,
compare or hash one; its reading agrees with Go's `ParseAddrPort` on the corpus of `tools/ip_oracle.go`.

An endpoint is an address, never a name: `"localhost:80"` is not one. [tcp::connect](tcp/connect.md) takes a name and
resolves it, and a [connection](connection.md) tells its two ends as endpoints. The empty endpoint, made by the
default constructor, has the empty address and the text `invalid AddrPort`, Go's.

## Rules

- **A plain value.** Thirty-four bytes, trivially copyable: it lives anywhere, and threads share a copy as they share
  an `int`.
- **The port is decimal**, 0 to 65535; leading zeros are accepted, a sign is not.
- **Brackets go around an IPv6 address and around nothing else**: `"::1:80"` and `"[1.2.3.4]:80"` are both refused. A
  zone is inside the brackets, and any byte of it is the zone's, a bracket included: `[fe80::1%a]b]:80` is an
  endpoint, as in Go, and round-trips.
- **Errors are values.** A text that is not an endpoint is an [io::error](../io/error.md) of the code
  `net::errc::invalid_address` ([errc](errc.md)).

## Member objects

| Constant | Value | Description |
|---|---|---|
| `MaxText` | `63` | the most bytes [write_text](endpoint/write_text.md) writes: the longest address with its zone, the brackets, `:` and five digits, `static constexpr size_t` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](endpoint/endpoint.md) | constructs the empty endpoint, an endpoint of an address and a port, or the endpoint a literal spells |

#### Observers

| Function | Description |
|---|---|
| [is_valid](endpoint/is_valid.md) | checks whether the endpoint has an address |
| [address](endpoint/address.md) | the address |
| [port](endpoint/port.md) | the port |

#### Text

| Function | Description |
|---|---|
| [parse](endpoint/parse.md) | reads an endpoint from a text (static) |
| [to_string](endpoint/to_string.md) | the text, `[::1]:443` |
| [write_text](endpoint/write_text.md) | the text into a buffer, with no string made |

#### Comparison

| Function | Description |
|---|---|
| [operator==, operator\<=\>](endpoint/operator_cmp.md) | compare two endpoints: the address, then the port |

## Non-member functions

| Function | Description |
|---|---|
| [format_value](endpoint/format_value.md) | writes the endpoint for [txt::format](../txt/format.md) and `println`, `{}` its `to_string()` |

## Specializations

```cpp
namespace std {
    template<> struct hash<sgcl::net::endpoint>;
}

template<>
struct sgcl::txt::formatter<sgcl::net::endpoint>;
```

The hash of the address and the port, `noexcept`: an endpoint is a key of a [map](../core/map.md) or a
[set](../core/set.md) as it is. The formatter tells [txt::format](../txt/format.md) which specifications an endpoint takes — the width, the fill
and the alignment, no type and no precision — so that a literal pattern is checked where it is compiled; the
writing is [format_value](endpoint/format_value.md)'s.

## Complexity

Constant for every member but [parse](endpoint/parse.md) and the constructor from a text, linear in the length of the
text, and the text members, linear in the length of the text written.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::endpoint web("[2001:db8::1]:443");
    println("{} {}", web.address(), web.port());

    net::endpoint local(net::ip_address::loopback_v4(), 8080);
    println("{}", local);

    for (const char* text : {"::1:80", "[1.2.3.4]:80", "localhost:80"}) {
        println("{}", net::endpoint::parse(text).has_value());
    }
}
```

Output:

```text
2001:db8::1 443
127.0.0.1:8080
false
false
false
```

## See also

- [ip_address](ip_address.md): the address; [ip_network](ip_network.md): an address and a prefix length
- [tcp](tcp.md), [udp](udp.md): where endpoints are dialed and listened on; [dns](dns.md): names to addresses
- [connection](connection.md): `local_endpoint` and `remote_endpoint`
- `tools/ip_oracle.go` (the oracle), `tests/net/ip.cpp`
