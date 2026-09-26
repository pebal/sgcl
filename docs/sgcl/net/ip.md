# sgcl::net::ip_address, sgcl::net::ip_network, sgcl::net::endpoint

```cpp
#include "sgcl/net/ip.h"   // or "sgcl/net/net.h"

namespace sgcl::net {
    class ip_address;      // IPv4 or IPv6, with the zone of a scoped address; 32 bytes, trivially copyable
    class ip_network;      // an address and a prefix length: "10.0.0.0/8"
    namespace net {
        class endpoint;    // an address and a port: "[::1]:443"
    }
}
// std::hash for the three
```

IP addresses as values, Go's `net/netip`: nothing allocated to parse (only a text that is not an address makes an error, which holds the text), copy, compare or hash one, so an address is a field, a key of a map, an element of a vector like an `int`. `parse` takes the forms of RFC 4291 §2.2 for IPv6 and only the dotted decimal form for IPv4; `to_string` writes RFC 5952. Both agree byte for byte with Go's `ParseAddr` and `String` on the corpus of `tools/ip_oracle.go` (the RFC examples, the edges, random addresses in several spellings and every one-character edit of them), as do the predicates, `next`, `prev`, the order, `endpoint::parse`, `net::ip_network::parse`, `masked`, `contains` and `overlaps`.

## Rules

- **IPv4 without leading zeros.** `"010.0.0.1"` is not an address: the C library reads a leading zero as octal, and a program that checked the text would disagree with the socket about where it connects (Go refuses it since 1.17). No `"127.1"`, no hex; the lenient forms of a browser belong to the URL parser (stage 1b).
- **IPv6 by RFC 4291 §2.2.** Groups of one to four hex digits, one `::` standing for one or more zero groups (never for none: `1:2:3:4:5:6:7:8::` is refused), an embedded IPv4 address in the last 32 bits, a zone after `%`. No brackets: those belong to an endpoint.
- **The text of RFC 5952.** Lower case; no leading zeros in a group; the longest run of two or more zero groups as `::`, the first of equal runs; one zero group written `0`; an IPv4-mapped address as `::ffff:1.2.3.4`.
- **An IPv4 address and its mapped form are two values**, as in Go: `net::ip_address::v4(1, 2, 3, 4) != *net::ip_address::parse("::ffff:1.2.3.4")`; `unmap()` turns the second into the first. The predicates look through the mapping (`::ffff:127.0.0.1` is loopback), except `is_unspecified`, true for `0.0.0.0` and `::` only.
- **The zone lives in the value**, at most fifteen bytes (an interface name is at most fifteen, `IFNAMSIZ` with its terminator). A longer zone does not parse, and `with_zone` of one throws `invalid_argument`: here this type differs from Go's, whose zone is a pointer to a string of any length; so does a zone with a NUL in it, refused here (a zone goes to `if_nametoindex` as a C string), kept by Go. Any other byte is a zone's, a bracket included: `[fe80::1%a]b]:80` is an endpoint in both, and round-trips. A numeric zone past the largest interface index names no interface (a dial of it is `net::errc::invalid_address`), never another by wrap-around. The predicates ignore the zone (`::%en0` is unspecified; Go says it is not).
- **The order** is the kind (the empty address, then IPv4, then IPv6), the bytes, the zone (none first).

## Members

### ip_address

```cpp
net::ip_address() noexcept;                                         // empty: !is_valid(), "invalid IP"
static expected<ip_address, io::error> parse(const string& text);         // "10.0.0.1", "2001:db8::1", "fe80::1%en0", "::ffff:1.2.3.4"
static net::ip_address v4(uint8_t a, uint8_t b, uint8_t c, uint8_t d) noexcept;
static net::ip_address v6(const array<uint8_t, 16>& bytes) noexcept;
static net::ip_address loopback_v4() noexcept;   // 127.0.0.1
static net::ip_address loopback_v6() noexcept;   // ::1
static net::ip_address any_v4() noexcept;        // 0.0.0.0
static net::ip_address any_v6() noexcept;        // ::

bool is_valid() const noexcept;
bool is_v4() const noexcept;
bool is_v6() const noexcept;
bool is_v4_mapped() const noexcept;         // ::ffff:a.b.c.d
net::ip_address unmap() const noexcept;          // ::ffff:1.2.3.4 -> 1.2.3.4; any other unchanged

bool is_loopback() const noexcept;          // 127.0.0.0/8, ::1
bool is_private() const noexcept;           // 10/8, 172.16/12, 192.168/16 (RFC 1918); fc00::/7 (RFC 4193)
bool is_unspecified() const noexcept;       // 0.0.0.0, ::
bool is_multicast() const noexcept;         // 224.0.0.0/4, ff00::/8
bool is_link_local() const noexcept;        // 169.254.0.0/16, fe80::/10 (unicast)
bool is_global_unicast() const noexcept;    // none of the above but private, and not 255.255.255.255

array<uint8_t, 16> bytes() const noexcept;  // network order; IPv4 as ::ffff:a.b.c.d
string zone() const;                        // "en0", or empty
bool has_zone() const noexcept;
net::ip_address with_zone(const string& zone) const;   // "" removes it; IPv4 unchanged; more than 15 bytes: invalid_argument
net::ip_address next() const noexcept;           // one above, the zone kept; the empty address past the last
net::ip_address prev() const noexcept;
string to_string() const;                   // RFC 5952, dotted decimal, "invalid IP"
auto operator<=>(const ip_address&) const noexcept = default;
```

```cpp
auto a = net::ip_address::parse("2001:0DB8:0000:0000:0000:0000:0000:0001");
a->to_string();                                     // "2001:db8::1"
net::ip_address::parse("010.0.0.1");                     // errc::invalid_address: a leading zero
net::ip_address::parse("::ffff:10.1.2.3")->is_private(); // true, through the mapping
net::ip_address::v4(192, 168, 1, 255).next();            // 192.168.2.0
```

### ip_network

```cpp
net::ip_network() noexcept;                                  // empty: !is_valid(), bits() == -1, "invalid Prefix"
net::ip_network(net::ip_address address, int bits);               // 0..32 for IPv4, 0..128 for IPv6, else invalid_argument; the zone dropped
static expected<ip_network, io::error> parse(const string& text);  // "10.0.0.0/8": no zone, bits in decimal without a sign or a leading zero
bool is_valid() const noexcept;
net::ip_address address() const noexcept;                    // as given: "10.1.2.3/8" keeps 10.1.2.3
int bits() const noexcept;
net::ip_network masked() const noexcept;                     // "10.1.2.3/8" -> "10.0.0.0/8"
bool contains(const ip_address& a) const noexcept;      // the same kind, no zone, the prefix equal
bool overlaps(const ip_network& o) const noexcept;
string to_string() const;
auto operator<=>(const ip_network&) const noexcept = default;
```

An IPv4-mapped address is not in an IPv4 network, nor an address with a zone in any: the question is asked of the value as it is, as Go asks it. `unmap()` first when the mapping should not matter.

```cpp
auto lan = *net::ip_network::parse("192.168.0.0/16");
lan.contains(net::ip_address::v4(192, 168, 7, 9));           // true
lan.overlaps(*net::ip_network::parse("192.168.4.0/22"));     // true
```

### net::endpoint

```cpp
endpoint() noexcept;                                    // empty: !is_valid(), "invalid AddrPort"
endpoint(net::ip_address address, uint16_t port) noexcept;
static expected<endpoint, io::error> parse(const string& text);    // "1.2.3.4:80", "[::1]:443", "[fe80::1%en0]:80"
net::ip_address address() const noexcept;
uint16_t port() const noexcept;
bool is_valid() const noexcept;
string to_string() const;                               // an IPv6 address in brackets, an IPv4 address never
auto operator<=>(const endpoint&) const noexcept = default;
```

The port is decimal, 0 to 65535; leading zeros are accepted, a sign is not. Brackets go around an IPv6 address and around nothing else: `"::1:80"` and `"[1.2.3.4]:80"` are both refused. A name (`"localhost:80"`) is not an endpoint; [`net::tcp::connect`](socket.md) takes one.

## See also

- [socket](socket.md): where endpoints are dialed and listened on; [dns](dns.md): names to addresses
- `tools/ip_oracle.go` (the oracle: `go run tools/ip_oracle.go > tests/net/ip_oracle.h`), `tests/net/ip.cpp`
