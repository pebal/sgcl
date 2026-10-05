[sgcl](../../README.md) › [net](../README.md) › [acme](README.md)

# sgcl::net::acme::external_account

```cpp
#include "sgcl/net/acme/types.h"   // or "sgcl/net/acme.h"

namespace sgcl::net::acme {
    struct external_account {
        string key_id;
        string hmac_key;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

External Account Binding (RFC 8555 §7.3.4): the key id and the MAC key a CA that requires it (ZeroSSL, Google Trust
Services, a CA of a company's own) gives its customers, which a new account signs its key with (HS256). Go's
`acme.ExternalAccountBinding`, its key as the CA prints it.

## Member objects

| Object | Description |
|---|---|
| `key_id` | the key id the CA gave |
| `hmac_key` | the MAC key, base64url as the CA gives it (padded or not) |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/acme.h"

using namespace sgcl;

int main() {
    net::acme::test_server ca({.require_external_account = true,
                               .external_accounts = {{"kid-1", "c2VjcmV0LWtleS1vZi10aGUtY2E"}}});
    net::acme::client acme(ca.directory_url(), net::acme::account_key());
    auto refused = acme.register_account({.terms_agreed = true});
    println("{}", refused.error().code() == net::acme::errc::external_account_required);
    net::acme::external_account binding{"kid-1", "c2VjcmV0LWtleS1vZi10aGUtY2E"};
    auto a = acme.register_account({.terms_agreed = true, .external_account = binding});
    println("{}", net::acme::to_string(a->status));
}
```

Output:

```text
true
valid
```

## See also

- [account_options](account_options.md)
- [net::acme](README.md)
