[sgcl](../README.md) › [net](README.md) › [dns](dns/README.md) › svc_param

# sgcl::net::dns::svc_param

```cpp
#include "sgcl/net/dns.h"   // or "sgcl/net.h"

namespace sgcl::net {
    struct dns {
        struct svc_param {
            uint16_t key = 0;
            vector<byte> value;

            friend bool operator==(const svc_param&, const svc_param&) noexcept = default;
        };
    };
}
```

**Requires [rooted](../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::dns::svc_param` is a SvcParam of an HTTPS or SVCB record (RFC 9460) of a key [svcb](dns-svcb.md) has no
field for: its key and its value's bytes as the record holds them. The keys with fields (0 to 7) never come here;
`ohttp` (8, RFC 9540), `tls-supported-groups` (9) and the keys registered later or of private use (65280–65534) do,
so a program can read a key the library does not know yet. A plain struct, compared field by field.

## Member objects

| Member | Description |
|---|---|
| `key` | the SvcParamKey; `0` by default |
| `value` | the value's bytes, in wire form (no presentation escapes); empty for a key without a value, as `ohttp` is |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::dns::svc_param ohttp{.key = 8};
    net::dns::svc_param groups{.key = 9, .value = {byte(0x00), byte(0x1d)}};  // X25519
    println("{} {} bytes", ohttp.key, ohttp.value.size());
    println("{} {} bytes, group {}", groups.key, groups.value.size(), int(groups.value[1]));
    println("{}", ohttp == net::dns::svc_param{8, {}});
}
```

Output:

```text
8 0 bytes
9 2 bytes, group 29
true
```

## See also

- [dns::svcb](dns-svcb.md): the record it belongs to
- [lookup_https, async_lookup_https](dns/lookup_https.md)
- [sgcl::net::dns](dns/README.md)
