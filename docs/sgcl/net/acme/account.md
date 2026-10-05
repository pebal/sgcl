[sgcl](../../README.md) › [net](../README.md) › [acme](README.md)

# sgcl::net::acme::account

```cpp
#include "sgcl/net/acme/types.h"   // or "sgcl/net/acme.h"

namespace sgcl::net::acme {
    struct account {
        string url;
        acme::status status = status::valid;
        vector<string> contact;
        bool terms_agreed = false;
        string orders;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

An account as the CA has it (RFC 8555 §7.1.2), what [register_account](client/register_account.md),
[account](client/account.md), [update_account](client/update_account.md) and
[deactivate_account](client/deactivate_account.md) give: Go's `acme.Account`.

## Member objects

| Object | Description |
|---|---|
| `url` | the account's URL, the `kid` every request of it is signed with |
| `status` | `valid`, `deactivated` or `revoked` ([status](status.md)) |
| `contact` | the URLs the CA reaches the account's holder at, `mailto:` ones |
| `terms_agreed` | whether the holder agreed to the CA's terms of service |
| `orders` | the URL of the list of its orders; empty when the CA gave none |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/acme.h"

using namespace sgcl;

int main() {
    net::acme::test_server ca;
    net::acme::client acme(ca.directory_url(), net::acme::account_key());
    net::acme::account a =
        acme.register_account({.contact = {"mailto:admin@example.com"}, .terms_agreed = true});
    println("{} {} {}", net::acme::to_string(a.status), a.contact[0], a.terms_agreed);
    println("{}", a.url == acme.account_url());
}
```

Output:

```text
valid mailto:admin@example.com true
true
```

## See also

- [account_options](account_options.md): what an account is made with
- [client](client/README.md)
- [net::acme](README.md)
