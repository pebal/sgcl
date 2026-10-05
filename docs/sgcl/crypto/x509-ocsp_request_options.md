[sgcl](../README.md) › [crypto](README.md) › [x509](x509.md)

# sgcl::crypto::x509::ocsp_request_options

```cpp
#include "sgcl/crypto/x509_revocation.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto::x509 {
    struct ocsp_request_options {
        hash_id hash = hash_id::sha1;
        bool nonce = false;
    };
}
```

`sgcl::crypto::x509::ocsp_request_options` is how [ocsp_request::make](x509-ocsp_request/make.md) makes a request, Go's
`ocsp.RequestOptions`: the hash of its CertID, and whether it carries a nonce. A call names what differs:
`ocsp_request::make(cert, issuer, {.nonce = true})`.

## Member objects

| Member | Description |
|---|---|
| `hash` | the hash of the issuer's name and key in the CertID ([hash_id](hash_id.md)): `sha1` by default, what every responder must take (RFC 5019 §2.1.1); `sha256`, `sha384`, `sha512` for a responder that takes them |
| `nonce` | whether the request carries a nonce of 16 random bytes (RFC 8954), which a responder that takes nonces echoes and [ocsp_verify_options](x509-ocsp_verify_options.md) then asks for; `false` by default: responders of the lightweight profile ignore nonces and answer from a cache (RFC 5019 §2.2.1) |

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string dir = "tests/crypto/data/revocation/";
    auto leaf = crypto::x509::certificate::from_pem(io::read_text(dir + "good.pem")).value();
    auto issuer = crypto::x509::certificate::from_pem(io::read_text(dir + "int.pem")).value();

    auto request = crypto::x509::ocsp_request::make(
        leaf, issuer, {.hash = crypto::hash_id::sha256, .nonce = true});
    println("{} {}", request->issuer_name_hash().size(), request->nonce().size());
}
```

Output:

```text
32 16
```

## See also

- [ocsp_request::make](x509-ocsp_request/make.md): what takes them
- [sgcl::crypto::x509](x509.md)
