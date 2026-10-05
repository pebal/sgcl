[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [certificate_request](README.md)

# sgcl::crypto::x509::certificate_request::parse

```cpp
[[nodiscard]] static expected<certificate_request, error> parse(const slice<const byte>& der)
    noexcept;
```

A certificate request in DER, Go's `x509.ParseCertificateRequest`: a CertificationRequest of version 1, its subject,
its SubjectPublicKeyInfo, its attributes (an extensionRequest read, the others passed over), the signature's algorithm
and the signature, all in strict DER and within a certificate's bounds (128 KiB, 64 extensions, 1024 names). The
signature is not checked: [check_signature](check_signature.md) does it.

## Parameters

| Parameter | Description |
|---|---|
| `der` | the bytes of the request |

## Return value

The request, or a [crypto::error](../error/README.md) `errc::malformed` with the offset of the byte where the reading
stopped.

## Complexity

Linear in the size of the request.

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
    auto made = request();
    auto read = crypto::x509::certificate_request::parse(made.raw());
    println("{}, {} names", read->subject().common_name(), read->dns_names().size());
    auto broken = crypto::x509::certificate_request::parse(made.raw().subslice(0, 10));
    println("{}", broken.error().code() == crypto::errc::malformed);
}
```

Output:

```text
example.com, 2 names
true
```

## See also

- [from_pem](from_pem.md): the request of a PEM text
- [sgcl::crypto::x509::certificate_request](README.md)
