[sgcl](../../README.md) › [net](../README.md)

# sgcl::net::ip_network

```cpp
#include "sgcl/net/ip.h"   // or "sgcl/net.h"

namespace sgcl::net {
    class ip_network;
}
```

`sgcl::net::ip_network` is a network in CIDR notation, an [ip_address](../ip_address/README.md) and the length of its prefix in
bits (`10.0.0.0/8`, `2001:db8::/32`): Go's `netip.Prefix`. It is a value as the address is, 34 bytes, trivially
copyable, nothing allocated to parse, copy, compare or hash one. Its reading, [masked](masked.md),
[contains](contains.md) and [overlaps](overlaps.md) agree with Go's on the corpus of
`tools/ip_oracle.go`.

The address is kept as given: `10.1.2.3/8` is a value of its own, not `10.0.0.0/8`, and [masked](masked.md)
clears the bits past the prefix. A network has no zone: it is not scoped to an interface, and an address given with
one loses it. The empty network, made by the default constructor, has the length -1 and the text `invalid Prefix`,
Go's.

## Rules

- **A plain value.** Thirty-four bytes, trivially copyable: it lives anywhere, and threads share a copy as they share
  an `int`.
- **The question is asked of the value as it is**, as Go asks it: an IPv4-mapped address is not in an IPv4 network,
  nor an address with a zone in any. [unmap](../ip_address/unmap.md) first when the mapping should not matter.
- **A length out of range is a broken contract.** `net::ip_network(a, 33)` of an IPv4 address throws
  `invalid_argument`, where Go's `PrefixFrom` returns an invalid prefix; a text with such a length is an error value
  of [parse](parse.md), `net::errc::invalid_address`.

## Member objects

| Constant | Value | Description |
|---|---|---|
| `MaxText` | `59` | the most bytes [write_text](write_text.md) writes: the longest address with its zone, `/` and three digits, `static constexpr size_t` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](ip_network.md) | constructs the empty network, a network of an address and a length, or the network a literal spells |

#### Observers

| Function | Description |
|---|---|
| [is_valid](is_valid.md) | checks whether the value is a network, not the empty one |
| [address](address.md) | the address, as given |
| [bits](bits.md) | the length of the prefix |

#### Operations

| Function | Description |
|---|---|
| [masked](masked.md) | the same network with the bits past the prefix cleared |
| [contains](contains.md) | checks whether an address lies in the network |
| [overlaps](overlaps.md) | checks whether two networks have an address in common |

#### Text

| Function | Description |
|---|---|
| [parse](parse.md) | reads a network from a text (static) |
| [to_string](to_string.md) | the text, `10.0.0.0/8` |
| [write_text](write_text.md) | the text into a buffer, with no string made |

#### Comparison

| Function | Description |
|---|---|
| [operator==, operator\<=\>](operator_cmp.md) | compare two networks: the address, then the length |

## Non-member functions

| Function | Description |
|---|---|
| [format_value](format_value.md) | writes the network for [txt::format](../../txt/format.md) and `println`, `{}` its `to_string()` |

## Specializations

```cpp
namespace std {
    template<> struct hash<sgcl::net::ip_network>;
}

template<>
struct sgcl::txt::formatter<sgcl::net::ip_network>;
```

The hash of the address and the length, `noexcept`: a network is a key of a [map](../../core/map/README.md) or a
[set](../../core/set/README.md) as it is. The formatter tells [txt::format](../../txt/format.md) which specifications a network takes — the width, the fill
and the alignment, no type and no precision — so that a literal pattern is checked where it is compiled; the
writing is [format_value](format_value.md)'s.

## Complexity

Constant for every member but [parse](parse.md) and the constructor from a text, linear in the length of
the text, and the text members, linear in the length of the text written.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::ip_network lan("192.168.0.0/16");
    println("{}", lan.contains(net::ip_address("192.168.7.9")));
    println("{}", lan.overlaps(net::ip_network("192.168.4.0/22")));

    net::ip_network host("10.1.2.3/8");
    println("{} {}", host, host.masked());

    auto wrong = net::ip_network::parse("10.0.0.0/33");
    println(wrong.error().message());
}
```

Output:

```text
true
true
10.1.2.3/8 10.0.0.0/8
parse IP network 10.0.0.0/33: invalid address
```

## See also

- [ip_address](../ip_address/README.md): the address; [endpoint](../endpoint/README.md): an address and a port
- `tools/ip_oracle.go` (the oracle), `tests/net/ip.cpp`
