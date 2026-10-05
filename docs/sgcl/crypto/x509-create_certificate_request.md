[sgcl](../README.md) › [crypto](README.md) › [x509](x509.md)

# sgcl::crypto::x509::create_certificate_request

```cpp
certificate_request create_certificate_request(const certificate_request_template& t,
                                               const signing_key& key);
```

A certificate request (PKCS #10, RFC 2986) of the fields of `t` for the public half of `key`, signed with it: Go's
`x509.CreateCertificateRequest`. The names go into an extensionRequest attribute holding a subjectAltName; the request
is read back by [certificate_request::parse](x509-certificate_request/parse.md). What a CA's finalize takes:
[acme::client::finalize](../net/acme/client/finalize.md) sends one.

## Parameters

| Parameter | Description |
|---|---|
| `t` | the subject and the names |
| `key` | the private key whose public half is asked to be certified, which signs |

## Return value

The request.

## Complexity

Linear in the size of the request, and one signature.

## Exceptions

- `std::invalid_argument` for a template that cannot be written: a name that is not ASCII or is empty, an IP address
  of neither 4 nor 16 bytes, an extension's OID that is not one or one given twice.
- `std::runtime_error` from an RSA signature that does not verify under its own key.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto key = crypto::p256::private_key::generate();
    crypto::x509::certificate_request_template t;
    t.dns_names = {"example.com"};
    auto csr = crypto::x509::create_certificate_request(t, key);
    println("{}, signature {}", csr.dns_names()[0], csr.check_signature().has_value());
    auto der = vector<byte>(csr.raw().begin(), csr.raw().end());
    string pem = encoding::pem("CERTIFICATE REQUEST", der).to_string();
    println("{}", pem.starts_with("-----BEGIN CERTIFICATE REQUEST-----"));
}
```

Output:

```text
example.com, signature true
true
```

## See also

- [certificate_request_template](x509-certificate_request_template.md): the fields
- [certificate_request](x509-certificate_request/README.md): a request read
- [x509](x509.md)
