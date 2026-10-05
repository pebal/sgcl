[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md)

# sgcl::crypto::x509::certificate_request

```cpp
#include "sgcl/crypto/x509.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto::x509 {
    class certificate_request;
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::crypto::x509::certificate_request` is a certificate request (PKCS #10, RFC 2986) read from DER or PEM: the
subject, the public key, the extensions it asks for and the names of their subjectAltName, and the check of its
signature under its own key. Go's `x509.CertificateRequest` as `ParseCertificateRequest` gives it; what a CA reads and
what [create_certificate_request](../x509-create_certificate_request.md) makes.

The request is read by the certificate's parser, over the same rules and bounds ([certificate](../x509-certificate/README.md)):
the extensions asked for are those of an extensionRequest attribute (PKCS #9 §5.4.2), the other attributes passed over.

## Rules

- **A value that costs a pointer**, as a [certificate](../x509-certificate/README.md) is: the parse is shared by every copy
  and never changed.
- **Made by [parse](parse.md) or [from_pem](from_pem.md)**, which return an expected: a request that cannot be read
  is `errc::malformed` with the offset, never an exception. There is no default constructor.
- **The signature is checked by [check_signature](check_signature.md)**, never by the parse: a request with a broken
  signature is read, for the program to report.

## Member functions

#### Reading

| Function | Description |
|---|---|
| [parse](parse.md) | a request from its DER (static) |
| [from_pem](from_pem.md) | the first request of a PEM text (static) |

#### Encoding

| Function | Description |
|---|---|
| [raw](raw.md) | the whole DER |
| [raw_tbs](raw_tbs.md) | the CertificationRequestInfo: what the signature covers |
| [raw_subject](raw_subject.md) | the subject's name as encoded |
| [raw_subject_public_key_info](raw_subject_public_key_info.md) | the SubjectPublicKeyInfo |

#### Fields

| Function | Description |
|---|---|
| [subject](subject.md) | the name of the subject |
| [public_key](public_key.md) | the subject's key |
| [signature_algorithm](signature_algorithm.md) | the algorithm of the signature |
| [signature](signature.md) | the signature |
| [extensions](extensions.md) | the extensions asked for, in order |

#### Subject alternative names

| Function | Description |
|---|---|
| [dns_names](dns_names.md) | the DNS names asked for |
| [ip_addresses](ip_addresses.md) | the IP addresses asked for |
| [email_addresses](email_addresses.md) | the email addresses asked for |
| [uris](uris.md) | the URIs asked for |

#### Verification

| Function | Description |
|---|---|
| [check_signature](check_signature.md) | checks that the request is signed by its own key |

## Non-member functions

| Function | Description |
|---|---|
| [operator==, operator!=](operator_cmp.md) | compare the bytes |

## See also

- [create_certificate_request](../x509-create_certificate_request.md): a request made
- [certificate](../x509-certificate/README.md): what a CA issues for it
- [x509](../x509.md)
