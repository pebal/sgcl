[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md)

# sgcl::crypto::x509::ocsp_request

```cpp
#include "sgcl/crypto/x509_revocation.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto::x509 {
    class ocsp_request;
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::crypto::x509::ocsp_request` is an OCSP request (RFC 6960 §4.1) of one certificate: the question a client asks
the responder its certificate names ([certificate::ocsp_servers](../x509-certificate/ocsp_servers.md)) — is this
certificate of this issuer revoked? What Go has as `ocsp.CreateRequest` and `ocsp.ParseRequest` of
`golang.org/x/crypto/ocsp`, made as a value with the CertID readable.

Sending it is the program's: a POST of [raw](raw.md) as `application/ocsp-request`, or the GET of [url](url.md), with
an [http::client](../../net/http/client/README.md); the answer is an [ocsp_response](../x509-ocsp_response/README.md).
[net::tls](../../net/tls/revocation_mode.md) does all of it itself when its config asks for revocation checks.

## Rules

- **A value that costs a pointer.** The request is one managed object, never changed: copies share it.
- **One certificate a request**, as Go's: [parse](parse.md) refuses a request of more.
- **Not signed.** RFC 6960 lets a request be signed; responders do not ask for it (RFC 5019 §2.1.1).

## Member functions

| Function | Description |
|---|---|
| [make](make.md) | the request of a certificate's status from its issuer's responder (static) |
| [parse](parse.md) | a request from its DER (static) |

#### Encoding

| Function | Description |
|---|---|
| [raw](raw.md) | the DER: the body of a POST |
| [url](url.md) | the GET of RFC 6960 Appendix A |

#### Fields

| Function | Description |
|---|---|
| [hash](hash.md) | the hash of the CertID |
| [issuer_name_hash](issuer_name_hash.md) | the hash of the issuer's name |
| [issuer_key_hash](issuer_key_hash.md) | the hash of the issuer's key |
| [serial_number](serial_number.md) | the certificate's serial number |
| [nonce](nonce.md) | the nonce, when there is one |
| [matches](matches.md) | checks whether the request names a certificate of an issuer |

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string dir = "tests/crypto/data/revocation/";
    auto leaf = crypto::x509::certificate::from_pem(io::read_text(dir + "good.pem")).value();
    auto issuer = crypto::x509::certificate::from_pem(io::read_text(dir + "int.pem")).value();

    auto request = crypto::x509::ocsp_request::make(leaf, issuer).value();
    println("{}", request.url(leaf.ocsp_servers()[0]));
    println("{}", request.matches(leaf, issuer));
}
```

Output:

```text
http://127.0.0.1:47811/MEMwQTA%2FMD0wOzAJBgUrDgMCGgUABBTzcZnxDtmD2Z8IJyr169PtS40KjAQUBz9j0XzHAoyEQVP%2FGrxM3sz0I%2BACAiAB
true
```

## See also

- [ocsp_response](../x509-ocsp_response/README.md): the answer
- [ocsp_request_options](../x509-ocsp_request_options.md): the hash and the nonce
- [net::tls::revocation_mode](../../net/tls/revocation_mode.md): the checks of TLS, which ask the responders themselves
- [sgcl::crypto::x509](../x509.md)
