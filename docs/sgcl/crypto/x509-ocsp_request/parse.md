[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [ocsp_request](README.md)

# sgcl::crypto::x509::ocsp_request::parse

```cpp
static expected<ocsp_request, error> parse(const slice<const byte>& der) noexcept;
```

Reads an OCSP request in DER, of exactly one certificate, as Go's `ocsp.ParseRequest` takes it: the CertID of its
Request, its nonce when it has one; a requestorName, extensions of the request other than the nonce and a signature
are read for their syntax. Strict DER, at most 1 MiB. A responder reads its requests so.

## Parameters

| Parameter | Description |
|---|---|
| `der` | the request's DER |

## Return value

The request, or `errc::malformed` with the offset for anything that is not one: more than one Request, data after it, DER that is not strict, past 1 MiB.

## Complexity

Linear in the size of the input.

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
    auto made = crypto::x509::ocsp_request::make(leaf, issuer, {.nonce = true}).value();
    auto read = crypto::x509::ocsp_request::parse(made.raw()).value();
    println("{} {}", read.matches(leaf, issuer), read.nonce() == made.nonce());
    println("{}", crypto::x509::ocsp_request::parse(slice<const byte>()).error().message());
}
```

Output:

```text
true true
sgcl::crypto::x509: not an OCSPRequest SEQUENCE
```

## See also

- [make](make.md): a request made
- [sgcl::crypto::x509::ocsp_request](README.md)
