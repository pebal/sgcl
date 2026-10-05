[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [certificate_request](README.md)

# sgcl::crypto::x509::certificate_request::raw

```cpp
SGCL_INLINE_HOT slice<const byte> raw() const noexcept;
```

The whole request, as the bytes it was read from: a view that keeps them alive.

## Parameters

None.

## Return value

The DER of the request.

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
    println("{}", csr.raw()[0] == byte(0x30));
}
```

Output:

```text
true
```

## See also

- [raw_tbs](raw_tbs.md): what the signature covers
- [sgcl::crypto::x509::certificate_request](README.md)
