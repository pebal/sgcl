[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [ocsp_request](README.md)

# sgcl::crypto::x509::ocsp_request::make

```cpp
static expected<ocsp_request, error> make(const certificate& cert, const certificate& issuer,
                                         const ocsp_request_options& o = {});
```

Makes the OCSP request (RFC 6960 §4.1.1) of the certificate's status from its issuer's responder: one Request whose
CertID names the certificate by its serial number and its issuer by the hashes of its subject and its key, with the
hash of `o` (SHA-1 by default, what a responder must take), and a nonce of 16 random bytes when `o` asks for one
(RFC 8954). The request is not signed. What `golang.org/x/crypto/ocsp` makes with `CreateRequest`.

## Parameters

| Parameter | Description |
|---|---|
| `cert` | the certificate whose status is asked |
| `issuer` | its issuer: the certificate whose subject is `cert`'s issuer |
| `o` | the hash and the nonce ([ocsp_request_options](../x509-ocsp_request_options.md)) |

## Return value

The request, or `errc::verification` with `reason::unknown_authority` when `issuer` is not `cert`'s issuer by name, `errc::malformed` when the issuer's key cannot be read.

## Complexity

Linear in the sizes of the issuer's name and key (their hashes).

## Exceptions

`std::invalid_argument` for a hash other than SHA-1, SHA-256, SHA-384 and SHA-512: the hash is the program's.

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
    println("{} bytes", request.raw().size());
    println("{}", crypto::x509::ocsp_request::make(leaf, leaf).error().message());
}
```

Output:

```text
69 bytes
sgcl::crypto::x509: the issuer given, "CN=good.localhost", is not the issuer of "CN=good.localhost"
```

## See also

- [url](url.md): the GET of the request
- [raw](raw.md): the body of a POST
- [ocsp_response::verify](../x509-ocsp_response/verify.md): the answer read
- [sgcl::crypto::x509::ocsp_request](README.md)
