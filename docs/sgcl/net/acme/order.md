[sgcl](../../README.md) › [net](../README.md) › [acme](README.md)

# sgcl::net::acme::order

```cpp
#include "sgcl/net/acme/types.h"   // or "sgcl/net/acme.h"

namespace sgcl::net::acme {
    struct order {
        string url;
        acme::status status = status::pending;
        optional<time::datetime> expires;
        vector<acme::identifier> identifiers;
        optional<time::datetime> not_before;
        optional<time::datetime> not_after;
        optional<problem> error;
        vector<string> authorizations;
        string finalize;
        string certificate;
        string replaces;
        string profile;
        duration retry_after = duration::zero();
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

An order of a certificate (RFC 8555 §7.1.3): `pending` until its authorizations are valid, `ready` to be finalized with
a CSR, `processing` while the CA issues, `valid` with the URL of its certificate, `invalid` when anything failed. What
[new_order](client/new_order.md), [order](client/order.md), [wait_order](client/wait_order.md) and
[finalize](client/finalize.md) give: Go's `acme.Order`.

## Member objects

| Object | Description |
|---|---|
| `url` | its URL |
| `status` | `pending`, `ready`, `processing`, `valid` or `invalid` ([status](status.md)) |
| `expires` | when the CA drops it |
| `identifiers` | the names it is for ([identifier](identifier.md)) |
| `not_before`, `not_after` | the validity asked for, when the order asked |
| `error` | why it is invalid ([problem](problem/README.md)) |
| `authorizations` | the URLs of its authorizations, one per identifier |
| `finalize` | the URL its CSR goes to |
| `certificate` | the URL of its certificate, once valid |
| `replaces` | the ARI id of the certificate it replaces (RFC 9773) |
| `profile` | the profile it asked for |
| `retry_after` | the CA's Retry-After with it, while it is processing; zero for none |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/acme.h"

using namespace sgcl;

int main() {
    net::acme::test_server ca({.skip_validation = true});
    net::acme::client acme(ca.directory_url(), net::acme::account_key());
    acme.register_account({.terms_agreed = true});
    net::acme::order o = acme.new_order({"example.com"});
    println("{}, {} authorization", net::acme::to_string(o.status), o.authorizations.size());
    acme.accept(acme.authorization(o.authorizations[0])->challenges[0]);
    println("{}", net::acme::to_string(acme.order(o.url)->status));
}
```

Output:

```text
pending, 1 authorization
ready
```

## See also

- [authorization](authorization.md), [order_options](order_options.md)
- [client::new_order](client/new_order.md)
- [net::acme](README.md)
