[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [certificate_request](README.md)

# sgcl::crypto::x509::certificate_request::dns_names

```cpp
SGCL_INLINE_HOT const vector<string>& dns_names() const noexcept;
```

The DNS names of the subjectAltName asked for, in their order.

## Parameters

None.

## Return value

The names.

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
    for (auto& n : csr.dns_names()) {
        println("{}", n);
    }
}
```

Output:

```text
example.com
www.example.com
```

## See also

- [ip_addresses](ip_addresses.md), [email_addresses](email_addresses.md), [uris](uris.md)
- [sgcl::crypto::x509::certificate_request](README.md)
