[sgcl](../README.md) › [crypto](README.md) › [x509](x509.md)

# sgcl::crypto::x509::create_certificate

```cpp
certificate create_certificate(const certificate_template& t,                                // (1)
                               const slice<const byte>& public_key_der,
                               const certificate& issuer, const signing_key& issuer_key);
certificate create_certificate(const certificate_template& t, const signing_key& key);       // (2)
```

A certificate (RFC 5280) of the fields of `t`, Go's `x509.CreateCertificate`: written in DER, signed, and read back by
[certificate::parse](x509-certificate/parse.md), so that what is returned is a certificate the module itself takes.

1. Issued by the CA `issuer` for the subject's public key `public_key_der` (a SubjectPublicKeyInfo: `to_pkix_der()`
   of any public key of the module), signed with `issuer_key`. The issuer's subject, as its certificate has it, is the
   certificate's issuer; its subjectKeyIdentifier, the authorityKeyIdentifier. The key is not checked against the
   issuer's certificate: one signed by another key does not verify.
2. Self-signed: the subject is the issuer, the key's public half the subject's key, a root of a test CA or of a
   private PKI.

## Parameters

| Parameter | Description |
|---|---|
| `t` | the fields |
| `public_key_der` | the subject's public key, a SubjectPublicKeyInfo in DER |
| `issuer` | the certificate of the CA that issues it |
| `issuer_key` | the CA's private key |
| `key` | the private key of a self-signed certificate |

## Return value

The certificate.

## Complexity

Linear in the size of the certificate, and one signature.

## Exceptions

- `std::invalid_argument` for a template that cannot be written: `not_after` before `not_before`, a serial past 20
  bytes or negative, a name of the subjectAltName that is not ASCII or is empty, an IP address of neither 4 nor 16
  bytes, an extension's OID that is not one or one given twice, a negative `max_path_length`, an extended key usage of
  no value of its enumeration, or a certificate past the parser's bounds (1024 names).
- `std::runtime_error` from an RSA signature that does not verify under its own key (a fault of the computation, as
  [rsa::private_key::sign_digest](rsa-private_key/sign_digest.md) checks).

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // a root, and a leaf for example.com it issues
    auto ca_key = crypto::p256::private_key::generate();
    crypto::x509::certificate_template ca;
    ca.common_name = "Example Root";
    ca.is_ca = true;
    auto root = crypto::x509::create_certificate(ca, ca_key);

    auto leaf_key = crypto::ed25519::private_key::generate();
    crypto::x509::certificate_template t;
    t.dns_names = {"example.com", "*.example.com"};
    t.ext_key_usages = {crypto::x509::ext_key_usage::server_auth};
    auto spki = leaf_key.public_key().to_pkix_der();
    auto leaf = crypto::x509::create_certificate(t, spki, root, ca_key);

    crypto::x509::certificate_pool roots;
    roots.add(root);
    auto chain = leaf.verify({.roots = roots, .dns_name = "www.example.com"});
    println("issued by {}, verified: {}", leaf.issuer().common_name(), chain.has_value());
}
```

Output:

```text
issued by Example Root, verified: true
```

## See also

- [certificate_template](x509-certificate_template.md): the fields and their defaults
- [create_certificate_request](x509-create_certificate_request.md): a request for a CA
- [x509](x509.md)
