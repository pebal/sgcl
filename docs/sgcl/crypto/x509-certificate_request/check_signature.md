[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [certificate_request](README.md)

# sgcl::crypto::x509::certificate_request::check_signature

```cpp
[[nodiscard]] expected<void, error> check_signature() const noexcept;
```

Checks that the request is signed by the private key of its own public key, Go's `CheckSignature`: the proof that
whoever asks for the certificate holds the key. The algorithms of a certificate's signatures are verified (RSA PKCS #1
v1.5 and PSS, ECDSA on P-256 and P-384, Ed25519; over SHA-256, SHA-384 and SHA-512), MD5 and SHA-1 never.

## Parameters

None.

## Return value

Nothing, or a [crypto::error](../error/README.md) `errc::verification` whose [reason](../x509-reason.md) is
`insecure_algorithm`, `unsupported_algorithm` or `invalid_signature`.

## Complexity

That of one verification of a signature.

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
    println("{}", csr.check_signature().has_value());
    auto bytes = vector<byte>(csr.raw().begin(), csr.raw().end());
    bytes[bytes.size() - 3] ^= byte(1);   // a bit of the signature changed
    auto altered = crypto::x509::certificate_request::parse(bytes);
    auto refused = altered->check_signature();
    println("{}", refused.error().reason() == crypto::x509::reason::invalid_signature);
}
```

Output:

```text
true
true
```

## See also

- [sgcl::crypto::x509::certificate_request](README.md)
