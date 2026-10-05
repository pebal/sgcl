[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [certificate_request](README.md)

# sgcl::crypto::x509::certificate_request::raw_subject_public_key_info

```cpp
SGCL_INLINE_HOT slice<const byte> raw_subject_public_key_info() const noexcept;
```

The SubjectPublicKeyInfo, as encoded: what [create_certificate](../x509-create_certificate.md) takes as the key of the
certificate it issues for the request.

## Parameters

None.

## Return value

The DER of the SubjectPublicKeyInfo.

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
    println("{} bytes", csr.raw_subject_public_key_info().size());
}
```

Output:

```text
91 bytes
```

## See also

- [public_key](public_key.md): the key read
- [sgcl::crypto::x509::certificate_request](README.md)
