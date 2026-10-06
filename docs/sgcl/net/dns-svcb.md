[sgcl](../README.md) › [net](README.md) › [dns](dns/README.md) › svcb

# sgcl::net::dns::svcb

```cpp
#include "sgcl/net/dns.h"   // or "sgcl/net.h"

namespace sgcl::net {
    struct dns {
        struct svcb {
            uint16_t priority = 0;
            string target;
            vector<string> alpn;
            bool no_default_alpn = false;
            uint16_t port = 0;
            vector<ip_address> ipv4_hints;
            vector<ip_address> ipv6_hints;
            vector<byte> ech;
            string dohpath;
            vector<uint16_t> mandatory;
            vector<svc_param> params;

            friend bool operator==(const svcb&, const svcb&) noexcept = default;
        };
    };
}
```

**Requires [rooted](../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::dns::svcb` is a service binding (RFC 9460), what [lookup_https](dns/lookup_https.md) and
[lookup_svcb](dns/lookup_svcb.md) give one of: an endpoint of a service, an HTTPS or SVCB record in ServiceMode, with
everything a client needs to reach it in one lookup — the protocols it speaks, its port, addresses to try first, and
the key of Encrypted Client Hello. Go's standard library has no such type (its resolver does not ask for these
records). A plain struct, compared field by field; the record's SvcParams are typed by key, and the keys it has no
field for kept as they came.

## Member objects

| Member | Description |
|---|---|
| `priority` | the order of the endpoints, the lowest first (1 to 65535); `0` only when an alias (AliasMode) led to a name without records of its own: connect to `target` as if there were no record; `0` by default |
| `target` | the endpoint's name, absolute, with its trailing dot; the record's `"."` already replaced by its owner's name (§2.5.2), so the target is always a name to connect to; empty by default |
| `alpn` | key 1: the protocols the endpoint speaks, the ALPN ids of TLS (`"h2"`, `"h3"`), in the record's order; empty when the record names none |
| `no_default_alpn` | key 2: the scheme's default protocol (`http/1.1` for HTTPS) is not among them; `false` by default |
| `port` | key 3: the endpoint's port; `0` when the record names none: the scheme's own (443 for HTTPS) |
| `ipv4_hints`, `ipv6_hints` | keys 4 and 6: addresses of the target a client may connect to before its A and AAAA answer; empty when none |
| `ech` | key 5: the ECHConfigList of Encrypted Client Hello (RFC 9848), what [tls::config](tls/config.md)`::ech_config_list` takes; empty when none |
| `dohpath` | key 7: the URI template of DNS over HTTPS on the endpoint (RFC 9461), relative (`"/dns-query{?dns}"`); empty when none |
| `mandatory` | key 0: the keys a client must understand to use the record, ascending; a client that does not know one of them skips the record (§8) |
| `params` | every other key ([svc_param](dns-svc_param.md)), ascending: `ohttp`, `tls-supported-groups`, the keys of later RFCs, the private keys 65280–65534 |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::dns::svcb s{.priority = 1, .target = "svc.example.com.", .alpn = {"h3", "h2"}, .port = 8443};
    s.ipv4_hints.push_back(net::ip_address::parse("192.0.2.1").value());
    println("{} {}:{} alpn {} first {}", s.priority, s.target, s.port, s.alpn.size(), s.alpn[0]);
    println("{} ech {}", s.ipv4_hints[0], s.ech.empty() ? "none" : "given");
    net::dns::svcb same = s;
    println("{}", same == s);
    same.port = 443;
    println("{}", same == s);
}
```

Output:

```text
1 svc.example.com.:8443 alpn 2 first h3
192.0.2.1 ech none
true
false
```

## See also

- [lookup_https, async_lookup_https](dns/lookup_https.md), [lookup_svcb, async_lookup_svcb](dns/lookup_svcb.md): what
  gives it, by priority
- [dns::svc_param](dns-svc_param.md): a key without a field
- [tls::config](tls/config.md): `ech_from_dns`, the client that takes `ech` from the record by itself
- [dns::srv](dns-srv.md): the older way to find a service's servers
- [sgcl::net::dns](dns/README.md)
