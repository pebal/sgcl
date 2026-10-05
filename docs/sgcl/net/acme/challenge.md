[sgcl](../../README.md) › [net](../README.md) › [acme](README.md)

# sgcl::net::acme::challenge

```cpp
#include "sgcl/net/acme/types.h"   // or "sgcl/net/acme.h"

namespace sgcl::net::acme {
    struct challenge {
        string type;
        string url;
        acme::status status = status::pending;
        string token;
        optional<time::datetime> validated;
        optional<problem> error;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

A challenge of an authorization (RFC 8555 §7.1.5, §8): one way to prove control of the name. `http-01` serves the key
authorization of `token` at [http01_path](client/http01_path.md) on port 80, `dns-01` publishes
[dns01_value](client/dns01_value.md) as the TXT record [dns01_name](client/dns01_name.md), `tls-alpn-01` serves
[tls_alpn01_identity](client/tls_alpn01_identity.md) on port 443 to a client offering `acme-tls/1`; then
[accept](client/accept.md) tells the CA to look. Go's `acme.Challenge`.

## Member objects

| Object | Description |
|---|---|
| `type` | `"http-01"`, `"dns-01"`, `"tls-alpn-01"`, or another type the CA has |
| `url` | its URL |
| `status` | `pending`, `processing`, `valid` or `invalid` ([status](status.md)) |
| `token` | the token the key authorization is made of (`http-01`, `dns-01`, `tls-alpn-01` have one) |
| `validated` | when the CA found it valid |
| `error` | why the CA found it invalid ([problem](problem/README.md)) |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/acme.h"

using namespace sgcl;

int main() {
    net::acme::test_server ca;
    net::acme::client acme(ca.directory_url(), net::acme::account_key());
    acme.register_account({.terms_agreed = true});
    net::acme::order o = acme.new_order({"example.com"});
    net::acme::authorization az = acme.authorization(o.authorizations[0]);
    for (auto& c : az.challenges) {
        string status = net::acme::to_string(c.status);
        println("{} {} token of {} characters", c.type, status, c.token.size());
    }
}
```

Output:

```text
http-01 pending token of 43 characters
dns-01 pending token of 43 characters
tls-alpn-01 pending token of 43 characters
```

## See also

- [authorization](authorization.md)
- [client::accept](client/accept.md)
- [net::acme](README.md)
