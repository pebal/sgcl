[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [certificate_request](README.md)

# sgcl::crypto::x509::certificate_request::ip_addresses

```cpp
SGCL_INLINE_HOT const vector<x509::ip_address>& ip_addresses() const noexcept;
```

The IP addresses of the subjectAltName asked for ([ip_address](../x509-ip_address/README.md)), 4 or 16 bytes each.

## Parameters

None.

## Return value

The addresses.

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
    auto& a = csr.ip_addresses()[0];
    println("{} bytes, {}.{}.{}.{}", a.size, int(a.bytes[0]), int(a.bytes[1]), int(a.bytes[2]),
            int(a.bytes[3]));
}
```

Output:

```text
4 bytes, 192.0.2.1
```

## See also

- [dns_names](dns_names.md)
- [sgcl::crypto::x509::certificate_request](README.md)
