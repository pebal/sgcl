[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [certificate_request](README.md)

# sgcl::crypto::x509::certificate_request::public_key

```cpp
SGCL_INLINE_HOT const x509::public_key& public_key() const noexcept;
```

The subject's key ([public_key](../x509-public_key/README.md)): one of the module's kinds, or `key_kind::none` for an
algorithm the module has no type for.

## Parameters

None.

## Return value

The key.

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
    println("{}", csr.public_key().kind() == crypto::x509::key_kind::p256);
}
```

Output:

```text
true
```

## See also

- [raw_subject_public_key_info](raw_subject_public_key_info.md): the key as encoded
- [sgcl::crypto::x509::certificate_request](README.md)
