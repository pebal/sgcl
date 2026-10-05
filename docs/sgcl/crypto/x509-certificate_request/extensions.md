[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [certificate_request](README.md)

# sgcl::crypto::x509::certificate_request::extensions

```cpp
SGCL_INLINE_HOT const vector<extension>& extensions() const noexcept;
```

The extensions of the extensionRequest attribute ([extension](../x509-extension.md)), in their order: what the request
asks to have in the certificate.

## Parameters

None.

## Return value

The extensions; empty for a request that asks for none.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

crypto::x509::certificate_request request() {
    auto key = crypto::p256::private_key::generate();
    crypto::x509::certificate_request_template t;
    t.common_name = "example.com";
    t.organization = {"Example, Inc."};
    t.dns_names = {"example.com", "www.example.com"};
    t.ip_addresses = {{.bytes = {byte(192), byte(0), byte(2), byte(1)}, .size = 4}};
    t.email_addresses = {"admin@example.com"};
    t.uris = {"https://example.com/"};
    return crypto::x509::create_certificate_request(t, key);
}

int main() {
    auto csr = request();
    println("{} {}", csr.extensions().size(), csr.extensions()[0].oid);
}
```

Output:

```text
1 2.5.29.17
```

## See also

- [dns_names](dns_names.md): the names of the subjectAltName
- [sgcl::crypto::x509::certificate_request](README.md)
