[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [certificate_request](README.md)

# sgcl::crypto::x509::operator==, operator!= (sgcl::crypto::x509::certificate_request)

```cpp
friend bool operator==(const certificate_request& a, const certificate_request& b) noexcept;
```

Compares the bytes of two requests: equal when they are the same request, or two of the same DER. `!=` is its
negation, as C++20 writes it.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the requests |

## Return value

`true` when the bytes are the same.

## Complexity

Linear in the size of the requests; constant for copies of one.

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
    auto a = request();
    auto again = crypto::x509::certificate_request::parse(a.raw());
    println("{} {}", a == *again, a != request());
}
```

Output:

```text
true true
```

## See also

- [sgcl::crypto::x509::certificate_request](README.md)
