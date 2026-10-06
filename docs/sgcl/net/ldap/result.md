[sgcl](../../README.md) › [net](../README.md) › [ldap](README.md)

# sgcl::net::ldap::result

```cpp
#include "sgcl/net/ldap/error.h"   // or "sgcl/net/ldap.h"

namespace sgcl::net::ldap {
    struct result {
        int code = 0;
        string matched_dn;
        string message;
        vector<string> referrals;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::ldap::result` is a server's answer to an operation (RFC 4511 §4.1.9), as [result_of](result_of.md) reads it back from an error: the code, the part of the DN the server found, its message, and the URLs of a referral.

## Member objects

| Member | Description |
|---|---|
| `code` | the result code: 32 no such object, 49 invalid credentials, ... ([errc](errc.md)) |
| `matched_dn` | the longest part of the DN asked for that exists, for a name that does not |
| `message` | the server's diagnostic message, often empty |
| `referrals` | the URLs of a referral (code 10) |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/ldap.h"

using namespace sgcl;

int main() {
    net::ldap::client::options o;
    o.security = net::ldap::security::none;  // slapd on this machine, no TLS
    net::ldap::client c = net::ldap::client::connect("ldap://localhost:3890", o).value();
    c.bind("cn=admin,dc=example,dc=com", "secret").value();
    auto r = c.search("ou=nobody,dc=example,dc=com", "(objectClass=*)");
    net::ldap::result res = net::ldap::result_of(r.error()).value();
    println("{}", res.code);
}
```

Output:

```text
32
```

## See also

- [result_of](result_of.md)
- [errc](errc.md)
