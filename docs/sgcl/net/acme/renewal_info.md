[sgcl](../../README.md) › [net](../README.md) › [acme](README.md)

# sgcl::net::acme::renewal_info

```cpp
#include "sgcl/net/acme/types.h"   // or "sgcl/net/acme.h"

namespace sgcl::net::acme {
    struct renewal_info {
        time::datetime start;
        time::datetime end;
        string explanation_url;
        duration retry_after = duration::zero();
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

When the CA suggests a certificate be renewed (RFC 9773 §4.2): a window, a point in which, chosen at random, spreads
the CA's load, and which a CA moves into the past to have a certificate replaced at once (a revocation coming, a key
compromised). What [renewal_info](client/renewal_info.md) gives; the [manager](manager/README.md) renews by it.

## Member objects

| Object | Description |
|---|---|
| `start`, `end` | the suggested window, `start` never after `end` |
| `explanation_url` | a page that says why the window is where it is; empty: none |
| `retry_after` | when to ask again (the CA's Retry-After) |

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
    acme.accept(acme.authorization(o.authorizations[0])->challenges[0]);
    auto key = crypto::p256::private_key::generate();
    crypto::x509::certificate_request_template t;
    t.dns_names = {"example.com"};
    auto csr = crypto::x509::create_certificate_request(t, key);
    net::acme::order done = acme.finalize(acme.wait_order(o.url), csr);
    auto leaf = acme.certificate(done.certificate)->certificates[0];
    net::acme::renewal_info info = acme.renewal_info(leaf);
    auto life = leaf.not_after().unix() - leaf.not_before().unix();
    println("from {:.2f} to {:.2f} of the lifetime, ask again in {}",
            double(info.start.unix() - leaf.not_before().unix()) / double(life),
            double(info.end.unix() - leaf.not_before().unix()) / double(life), info.retry_after);
}
```

Output:

```text
from 0.67 to 0.78 of the lifetime, ask again in 6h0m0s
```

## See also

- [client::renewal_info](client/renewal_info.md)
- [net::acme](README.md)
