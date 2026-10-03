[sgcl](../README.md) › [net](README.md)

# sgcl::net::ip_address

```cpp
#include "sgcl/net/ip.h"   // or "sgcl/net.h"

namespace sgcl::net {
    class ip_address;
}
```

`sgcl::net::ip_address` is an IP address as a value, what Go's `netip.Addr` is: an IPv4 or an IPv6 address, with the
zone of a scoped IPv6 address (`fe80::1%en0`). It is sixteen bytes of address, the kind (none, IPv4, IPv6) and the
zone, thirty-two bytes in all and trivially copyable: nothing is allocated to parse, copy, compare or hash one (only a
text that is not an address makes an error, which holds the text), so an address is a field, a key of a map, an
element of a vector as an `int` is. Go's `netip.Addr` is 24 bytes, its zone a pointer to an interned string; here the
zone lives in the value, at most fifteen bytes.

[parse](ip_address/parse.md) takes the forms of RFC 4291 §2.2 for IPv6 and only the dotted decimal form for IPv4;
[to_string](ip_address/to_string.md) writes the text of RFC 5952. Both agree byte for byte with Go's `ParseAddr` and
`String` on the corpus of `tools/ip_oracle.go` (the RFC examples, the edges, random addresses in several spellings and
every one-character edit of them), as do the predicates, [next](ip_address/next.md), [prev](ip_address/prev.md) and
the order.

The empty address, made by the default constructor, is no address: `is_valid()` is false and its text is `invalid IP`,
Go's. An address with a port is an [endpoint](endpoint.md), an address with a prefix length an
[ip_network](ip_network.md); a name is neither, and goes to [dns::lookup](dns/lookup.md).

## Rules

- **A plain value.** Thirty-two bytes, trivially copyable, every member `noexcept` but the constructor from a text and
  `with_zone`: it lives anywhere, a stack, a global, a `std` container, a managed object, and threads share a copy as
  they share an `int`.
- **IPv4 without leading zeros.** `"010.0.0.1"` is not an address: the C library reads a leading zero as octal, and a
  program that checked the text would disagree with the socket about where it connects (Go refuses it since 1.17). No
  `"127.1"`, no hex; the lenient forms of a browser belong to the URL parser ([url](url.md)).
- **IPv6 by RFC 4291 §2.2.** Groups of one to four hex digits, one `::` standing for one or more zero groups (never
  for none: `1:2:3:4:5:6:7:8::` is refused), an embedded IPv4 address in the last 32 bits, a zone after `%`. No
  brackets: those belong to an endpoint.
- **The text of RFC 5952.** Lower case; no leading zeros in a group; the longest run of two or more zero groups as
  `::`, the first of equal runs; one zero group written `0`; an IPv4-mapped address as `::ffff:1.2.3.4`.
- **An IPv4 address and its mapped form are two values**, as in Go: `net::ip_address::v4(1, 2, 3, 4) !=
  net::ip_address("::ffff:1.2.3.4")`; [unmap](ip_address/unmap.md) turns the second into the first. The predicates
  look through the mapping (`::ffff:127.0.0.1` is loopback), except `is_unspecified`, true for `0.0.0.0` and `::`
  only.
- **The zone lives in the value**, at most fifteen bytes (an interface name is at most fifteen, `IFNAMSIZ` with its
  terminator). A longer zone does not parse, and [with_zone](ip_address/with_zone.md) of one throws
  `invalid_argument`: here this type differs from Go's, whose zone is a pointer to a string of any length; so does a
  zone with a NUL in it, refused here (a zone goes to `if_nametoindex` as a C string), kept by Go. Any other byte is a
  zone's, a bracket included: `[fe80::1%a]b]:80` is an endpoint in both, and round-trips. A numeric zone past the
  largest interface index names no interface (a dial of it is `net::errc::invalid_address`), never another by
  wrap-around. The predicates ignore the zone (`::%en0` is unspecified; Go says it is not).
- **The order** is the kind (the empty address, then IPv4, then IPv6), the bytes, the zone (none first).
- **Errors are values.** A text that is not an address is an [io::error](../io/error.md) of the code
  `net::errc::invalid_address` ([errc](errc.md)); an exception is a broken contract only: a zone of more than fifteen
  bytes given to `with_zone`, or a literal the [constructor](ip_address/ip_address.md) cannot read.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](ip_address/ip_address.md) | constructs the empty address, or the address a literal spells |

#### Making

| Function | Description |
|---|---|
| [v4](ip_address/v4.md) | the IPv4 address of four bytes (static) |
| [v6](ip_address/v6.md) | the IPv6 address of sixteen bytes (static) |

#### Special values

| Function | Description |
|---|---|
| [loopback_v4](ip_address/loopback_v4.md) | `127.0.0.1` (static) |
| [loopback_v6](ip_address/loopback_v6.md) | `::1` (static) |
| [any_v4](ip_address/any_v4.md) | `0.0.0.0` (static) |
| [any_v6](ip_address/any_v6.md) | `::` (static) |

#### Kind

| Function | Description |
|---|---|
| [is_valid](ip_address/is_valid.md) | checks whether the value is an address, not the empty one |
| [is_v4](ip_address/is_v4.md) | checks whether the address is IPv4 |
| [is_v6](ip_address/is_v6.md) | checks whether the address is IPv6, an IPv4-mapped one among them |
| [is_v4_mapped](ip_address/is_v4_mapped.md) | checks whether the address is IPv4-mapped, `::ffff:a.b.c.d` |
| [unmap](ip_address/unmap.md) | the IPv4 address of an IPv4-mapped one |

#### Classification

| Function | Description |
|---|---|
| [is_loopback](ip_address/is_loopback.md) | `127.0.0.0/8`, `::1` |
| [is_private](ip_address/is_private.md) | RFC 1918 and RFC 4193: `10/8`, `172.16/12`, `192.168/16`, `fc00::/7` |
| [is_unspecified](ip_address/is_unspecified.md) | `0.0.0.0`, `::` |
| [is_multicast](ip_address/is_multicast.md) | `224.0.0.0/4`, `ff00::/8` |
| [is_link_local](ip_address/is_link_local.md) | link-local unicast: `169.254.0.0/16`, `fe80::/10` |
| [is_global_unicast](ip_address/is_global_unicast.md) | none of the above but private, and not the IPv4 broadcast address |

#### Bytes and zone

| Function | Description |
|---|---|
| [bytes](ip_address/bytes.md) | the sixteen bytes in network order |
| [zone](ip_address/zone.md) | the zone, `en0` of `fe80::1%en0` |
| [has_zone](ip_address/has_zone.md) | checks whether the address has a zone |
| [with_zone](ip_address/with_zone.md) | the address with another zone, or without one |

#### Neighbours

| Function | Description |
|---|---|
| [next](ip_address/next.md) | the address one above |
| [prev](ip_address/prev.md) | the address one below |

#### Text

| Function | Description |
|---|---|
| [parse](ip_address/parse.md) | reads an address from a text (static) |
| [to_string](ip_address/to_string.md) | the text of RFC 5952, or dotted decimal |

#### Comparison

| Function | Description |
|---|---|
| [operator==, operator\<=\>](ip_address/operator_cmp.md) | compare two addresses: the kind, the bytes, the zone |

## Non-member functions

| Function | Description |
|---|---|
| [format_value](ip_address/format_value.md) | writes the address for [txt::format](../txt/format.md) and `println`, `{}` its `to_string()` |

## Specializations

```cpp
namespace std {
    template<> struct hash<sgcl::net::ip_address>;
}

template<>
struct sgcl::txt::formatter<sgcl::net::ip_address>;
```

The hash of the thirty-two bytes, `noexcept`: an address is a key of a [map](../core/map.md) or a
[set](../core/set.md) as it is, with nothing allocated. The formatter tells [txt::format](../txt/format.md) which
specifications an address takes — the width, the fill and the alignment, no type and no precision — so that a
literal pattern is checked where it is compiled; the writing is [format_value](ip_address/format_value.md)'s.

## Complexity

Constant for every member but [parse](ip_address/parse.md) and the constructor from a text, linear in the length of
the text, and [to_string](ip_address/to_string.md), linear in the length of the address's text (at most 55 bytes).

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::ip_address a("2001:0DB8:0000:0000:0000:0000:0000:0001");
    println("{}", a);

    auto octal = net::ip_address::parse("010.0.0.1");
    println(octal.error().message());

    println("{}", net::ip_address("::ffff:10.1.2.3").is_private());  // through the mapping
    println("{}", net::ip_address::v4(192, 168, 1, 255).next());

    set<net::ip_address> seen;  // std::hash: an address is a key
    for (const char* text : {"10.0.0.1", "10.0.0.2", "10.0.0.1"}) {
        seen.insert(net::ip_address(text));
    }
    println("{}", seen.size());
}
```

Output:

```text
2001:db8::1
parse IP address 010.0.0.1: invalid address
true
192.168.2.0
2
```

## See also

- [ip_network](ip_network.md): an address and a prefix length; [endpoint](endpoint.md): an address and a port
- [dns::lookup](dns/lookup.md): the addresses of a name; [tcp::connect](tcp/connect.md): where an address is dialed
- [url::host_address](url/host_address.md): the address of a URL's host
- `tools/ip_oracle.go` (the oracle: `go run tools/ip_oracle.go > tests/net/ip_oracle.h`), `tests/net/ip.cpp`
