[sgcl](../../../README.md) › [net](../../README.md) › [acme](../README.md) › [test_server](README.md)

# sgcl::net::acme::test_server::roots

```cpp
crypto::x509::certificate_pool roots() const noexcept;
```

The roots of the server's CA, each chain's: what verifies the certificates it issues, and its own over https.

## Parameters

None.

## Return value

The pool (a handle: its copies the same pool).

## Complexity

Constant.

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
    auto chain = certificates.certificate("example.com")->certificates();
    crypto::x509::certificate_pool intermediates;
    intermediates.add(chain[1]);
    auto verified = chain[0].verify(
        {.roots = ca.roots(), .intermediates = intermediates, .dns_name = "example.com"});
    println("{}", verified.has_value());
    certificates.close();
}
```

Output:

```text
true
```

## See also

- [crypto::x509::certificate_pool](../../../crypto/x509-certificate_pool/README.md)
- [sgcl::net::acme::test_server](README.md)
