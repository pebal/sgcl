[sgcl](../../README.md) › [net](../README.md) › [acme](README.md)

# sgcl::net::acme::order_options

```cpp
#include "sgcl/net/acme/types.h"   // or "sgcl/net/acme.h"

namespace sgcl::net::acme {
    struct order_options {
        optional<time::datetime> not_before;
        optional<time::datetime> not_after;
        string replaces;
        string profile;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

What a new order asks for besides its names (RFC 8555 §7.4, RFC 9773 §5, the profiles extension), what
[new_order](client/new_order.md) sends; Go's `acme.OrderOption`s as fields.

## Member objects

| Object | Description |
|---|---|
| `not_before`, `not_after` | the validity asked for; none: the CA's (Let's Encrypt refuses both) |
| `replaces` | the ARI id of a certificate this one replaces ([renewal_id](client/renewal_id.md)), which a CA with renewalInfo counts as a renewal; `errc::unsupported` without it |
| `profile` | a profile of [directory](directory.md)`::profiles`; empty: the CA's default |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/acme.h"

using namespace sgcl;

int main() {
    net::acme::test_server::options o;
    o.profiles["shortlived"] = "six days";
    net::acme::test_server ca(o);
    net::acme::client acme(ca.directory_url(), net::acme::account_key());
    acme.register_account({.terms_agreed = true});
    println("{}", acme.new_order({"example.com"}, {.profile = "shortlived"})->profile);
    println("{}", acme.new_order({"example.com"}, {.profile = "classic"}).error().message());
}
```

Output:

```text
shortlived
acme new-order no profile classic: acme: the profile is not one the CA offers
```

## See also

- [order](order.md)
- [net::acme](README.md)
