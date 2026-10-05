[sgcl](../../README.md) › [net](../README.md) › [dns_sd](README.md)

# sgcl::net::dns_sd::service

```cpp
#include "sgcl/net/mdns.h"   // or "sgcl/net.h"

namespace sgcl::net::dns_sd {
    struct service {
        string name;
        string type;
        string domain;
        string host;
        uint16_t port = 0;
        txt_record txt;
        vector<string> subtypes;
        vector<ip_address> addresses;
        uint32_t interface = 0;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::dns_sd::service` is a service instance of DNS-SD (RFC 6763 §4.1): what [resolve](resolve.md) gives and
what [publish](publish.md) takes. Its name, type and domain make the instance's full name
(`Living Room._airplay._tcp.local.`), its host and port are the SRV's, its [txt_record](txt_record/README.md) the TXT;
to publish, its subtypes; found, the host's addresses and the interface it answered on. A plain struct of fields.

## Member objects

| Member | Description |
|---|---|
| `name` | the instance's name: any UTF-8, at most 63 bytes, dots and spaces among them (`"Living Room"`) |
| `type` | its type: `"_http._tcp"`, `"_ipp._tcp"`, `"_airplay._tcp"` |
| `domain` | its domain, absolute; empty, the default, to publish: `"local."`; found: `"local."` |
| `host` | the host the service runs on, absolute (`"mymac.local."`); to publish, empty, the default: the responder's |
| `port` | the port the service listens on; 0 by default |
| `txt` | the TXT record: keys with values or alone; empty by default (published as one empty string, §6.1) |
| `subtypes` | to publish: the subtypes it is also browsed by (`"_printer"`, §7.1); empty by default; not filled by resolve |
| `addresses` | found: the host's addresses, IPv4's first, an IPv6 link-local one with its zone; not read by publish |
| `interface` | found: the index of the interface its SRV came on; 0 by default; not read by publish |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::dns_sd::service s;
    s.name = "Living Room";
    s.type = "_http._tcp";
    s.port = 80;
    s.txt = {{"path", "/"}};
    s.subtypes = {"_printer"};
    println("{} {} {} {} {}", s.name, s.type, s.port, s.txt.size(), s.subtypes.size());
    println("{}", s.domain.empty() && s.host.empty() && s.addresses.empty());
}
```

Output:

```text
Living Room _http._tcp 80 1 1
true
```

## See also

- [resolve](resolve.md), [publish](publish.md): what gives it, what takes it
- [sgcl::net::dns_sd](README.md)
