[sgcl](../../README.md) › [net](../README.md) › [acme](README.md)

# sgcl::net::acme::directory

```cpp
#include "sgcl/net/acme/types.h"   // or "sgcl/net/acme.h"

namespace sgcl::net::acme {
    struct directory {
        string new_nonce;
        string new_account;
        string new_order;
        string new_authz;
        string revoke_cert;
        string key_change;
        string renewal_info;
        string terms_of_service;
        string website;
        vector<string> caa_identities;
        bool external_account_required = false;
        ordered_map<string, string> profiles;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

The resources of a CA and what it says of itself (RFC 8555 §7.1.1), what [directory](client/directory.md) reads once:
Go's `acme.Directory`. The URLs are absolute, resolved against the directory's own.

## Member objects

| Object | Description |
|---|---|
| `new_nonce`, `new_account`, `new_order` | the resources every CA has |
| `new_authz` | pre-authorization; empty when not offered (Let's Encrypt does not) |
| `revoke_cert`, `key_change` | revocation and key rollover |
| `renewal_info` | renewal information (RFC 9773); empty when not offered |
| `terms_of_service` | the URL of the CA's terms, which an account agrees to |
| `website` | the CA's site |
| `caa_identities` | the names the CA recognizes in CAA records |
| `external_account_required` | whether a new account needs an [external_account](external_account.md) |
| `profiles` | the certificate profiles the CA offers, by name, with their descriptions (the profiles extension) |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/acme.h"

using namespace sgcl;

int main() {
    net::acme::test_server ca({.terms_of_service = "https://ca.example/terms"});
    net::acme::client acme(ca.directory_url(), net::acme::account_key());
    net::acme::directory d = acme.directory();
    println("{}", d.terms_of_service);
    println("{} {} {}", d.renewal_info.empty(), d.new_authz.empty(), d.external_account_required);
}
```

Output:

```text
https://ca.example/terms
false true false
```

## See also

- [client::directory](client/directory.md)
- [net::acme](README.md)
