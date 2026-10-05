[sgcl](../../README.md) › [net](../README.md) › [acme](README.md)

# sgcl::net::acme::account_options

```cpp
#include "sgcl/net/acme/types.h"   // or "sgcl/net/acme.h"

namespace sgcl::net::acme {
    struct account_options {
        vector<string> contact;
        bool terms_agreed = false;
        optional<acme::external_account> external_account;
        bool only_return_existing = false;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

What a new account is made with (RFC 8555 §7.3), what [register_account](client/register_account.md) sends: Go's
`acme.Account` given to `Register`, its `prompt` a field here. A designated initializer names what differs:
`acme.register_account({.terms_agreed = true})`.

## Member objects

| Object | Description |
|---|---|
| `contact` | the URLs the CA reaches the holder at, `"mailto:admin@example.com"`; empty by default |
| `terms_agreed` | the CA's terms of service ([directory](directory.md)`::terms_of_service`) agreed to; a CA with terms refuses an account without it (Let's Encrypt does); `false` by default |
| `external_account` | the binding a CA that requires one gave ([external_account](external_account.md)); none by default |
| `only_return_existing` | the account of the key if it has one, never a new one: `errc::account_does_not_exist` when it has none; `false` by default |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/acme.h"

using namespace sgcl;

int main() {
    net::acme::test_server ca;
    net::acme::account_key key;
    net::acme::client acme(ca.directory_url(), key);
    auto none = acme.register_account({.only_return_existing = true});
    println("{}", none.error().message());
    acme.register_account({.terms_agreed = true});
    auto found = acme.register_account({.only_return_existing = true});
    println("{}", found->url == acme.account_url());
}
```

Output:

```text
acme new-account no account of this key: acme: the account does not exist
true
```

## See also

- [account](account.md)
- [client::register_account](client/register_account.md)
- [net::acme](README.md)
