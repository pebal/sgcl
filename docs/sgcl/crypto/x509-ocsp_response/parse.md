[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [ocsp_response](README.md)

# sgcl::crypto::x509::ocsp_response::parse

```cpp
static expected<ocsp_response, error> parse(const slice<const byte>& der) noexcept;
```

Reads an OCSP response in DER (RFC 6960 §4.2): its status, and of a successful one the BasicOCSPResponse inside —
the responder, the time it was produced, every SingleResponse, the extensions and the nonce among them, the signature
and the certificates the responder sent. Strict DER, at most 1 MiB, 256 single responses and 16 certificates. A
response that is not successful (`tryLater`, `unauthorized`...) parses too and holds nothing but its status.
Nothing is verified: [verify](verify.md) does that.

## Parameters

| Parameter | Description |
|---|---|
| `der` | the response's DER, as a responder answers it (`application/ocsp-response`) |

## Return value

The response, or `errc::malformed` with the offset for anything that is not one or passes the bounds, `errc::unsupported` for a response of a type other than `id-pkix-ocsp-basic`.

## Complexity

Linear in the size of the input, and in the certificates it holds (each parsed).

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string dir = "tests/crypto/data/revocation/";
    auto leaf = crypto::x509::certificate::from_pem(io::read_text(dir + "good.pem")).value();
    auto issuer = crypto::x509::certificate::from_pem(io::read_text(dir + "int.pem")).value();
    auto response = crypto::x509::ocsp_response::parse(
        io::read_file(dir + "ocsp_good.der").value()).value();
    bool successful = response.status() == crypto::x509::ocsp_response_status::successful;
    println("{} {}", successful, response.responses().size());
    println("{}", crypto::x509::ocsp_response::parse(
        io::read_file(dir + "good.pem").value()).error().message());
}
```

Output:

```text
true 1
sgcl::crypto::x509: not an OCSPResponse SEQUENCE
```

## See also

- [verify](verify.md): the response checked for a certificate
- [sgcl::crypto::x509::ocsp_response](README.md)
