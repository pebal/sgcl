[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md)

# sgcl::crypto::x509::ocsp_response

```cpp
#include "sgcl/crypto/x509_revocation.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto::x509 {
    class ocsp_response;
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::crypto::x509::ocsp_response` is an OCSP response (RFC 6960 §4.2): a responder's signed answer about the
revocation of one or more certificates, read from DER, and verified for a certificate and its issuer. What a responder
answers an [ocsp_request](../x509-ocsp_request/README.md) with, and what a TLS server staples to its chain. Go has it as
`golang.org/x/crypto/ocsp`'s `Response`, whose `ParseResponseForCert` reads and checks the signature in one call and
leaves the times to its caller; here [parse](parse.md) reads, and [verify](verify.md) checks everything a client
must: the signer, the signature, the certificate, the times, the nonce.

## Rules

- **A value that costs a pointer.** The parse is one managed object, never changed: copies share it, and it is read
  from many threads at once without a lock.
- **Never an exception from data.** A response that cannot be read is `errc::malformed` from [parse](parse.md); one
  that does not verify, `errc::verification` with a [reason](../x509-reason.md) from [verify](verify.md).
- **The status is the responder's.** [verify](verify.md) checks that the answer is authentic and current; a revoked
  certificate verifies, its [single response](../x509-ocsp_single_response.md) saying `revoked`.
- **Bounded.** At most 1 MiB, 256 single responses and 16 certificates.

## Member functions

| Function | Description |
|---|---|
| [parse](parse.md) | a response from its DER (static) |

#### Encoding

| Function | Description |
|---|---|
| [raw](raw.md) | the whole DER |
| [raw_tbs](raw_tbs.md) | the ResponseData: what the signature covers |

#### Fields

| Function | Description |
|---|---|
| [status](status.md) | whether the responder answered with a status |
| [responder_name](responder_name.md) | the responder by its name |
| [responder_key_hash](responder_key_hash.md) | the responder by its key |
| [produced_at](produced_at.md) | when the responder signed it |
| [responses](responses.md) | every single response |
| [certificates](certificates.md) | the certificates the responder sent |
| [extensions](extensions.md) | the response's extensions |
| [nonce](nonce.md) | the nonce, when there is one |
| [signature_algorithm](signature_algorithm.md) | the algorithm of the signature |
| [signature_algorithm_oid](signature_algorithm_oid.md) | the algorithm's OID |
| [signature](signature.md) | the responder's signature |

#### Verification

| Function | Description |
|---|---|
| [check_signature_from](check_signature_from.md) | checks whether a certificate's key signed the response |
| [verify](verify.md) | the single response of a certificate, verified |

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string dir = "tests/crypto/data/revocation/";
    auto leaf = crypto::x509::certificate::from_pem(io::read_text(dir + "revoked.pem")).value();
    auto issuer = crypto::x509::certificate::from_pem(io::read_text(dir + "int.pem")).value();

    auto response = crypto::x509::ocsp_response::parse(
        io::read_file(dir + "ocsp_revoked.der").value()).value();
    auto single = response.verify(leaf, issuer).value();
    println("{}", single.status == crypto::x509::revocation_status::revoked);
    println("{}", single.reason == crypto::x509::revocation_reason::key_compromise);
}
```

Output:

```text
true
true
```

## See also

- [ocsp_request](../x509-ocsp_request/README.md): the question
- [ocsp_single_response](../x509-ocsp_single_response.md): the status of a certificate
- [revocation_list](../x509-revocation_list/README.md): the CA's whole list instead
- [net::tls::identity::set_ocsp_staple](../../net/tls/identity/set_ocsp_staple.md): a response a server staples
- [sgcl::crypto::x509](../x509.md)
