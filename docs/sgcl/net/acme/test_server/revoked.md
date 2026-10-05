[sgcl](../../../README.md) › [net](../../README.md) › [acme](../README.md) › [test_server](README.md)

# sgcl::net::acme::test_server::revoked

```cpp
bool revoked(const crypto::x509::certificate& cert) const noexcept;
```

Whether the server's CA revoked the certificate.

## Parameters

| Parameter | Description |
|---|---|
| `cert` | a certificate |

## Return value

`true` for a certificate it issued and revoked.

## Complexity

Linear in the certificates issued.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/acme.h"

using namespace sgcl;

int main() {
    net::acme::test_server ca({.skip_validation = true});
    net::acme::manager certificates({"example.com"},
                                    {.directory_url = ca.directory_url(), .accept_terms = true});
    auto leaf = certificates.certificate("example.com")->certificates()[0];
    println("{}", ca.revoked(leaf));
    certificates.client()->revoke(leaf);
    println("{}", ca.revoked(leaf));
    certificates.close();
}
```

Output:

```text
false
true
```

## See also

- [client::revoke](../client/revoke.md)
- [sgcl::net::acme::test_server](README.md)
