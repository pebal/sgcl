[sgcl](../README.md) › [crypto](README.md) › [x509](x509.md)

# sgcl::crypto::x509::ocsp_response_status

```cpp
#include "sgcl/crypto/x509_revocation.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto::x509 {
    enum class ocsp_response_status : uint8_t {
        successful = 0,
        malformed_request = 1,
        internal_error = 2,
        try_later = 3,
        sig_required = 5,
        unauthorized = 6
    };
}
```

Whether an OCSP responder answered with a status, or why not: the OCSPResponseStatus of RFC 6960 §4.2.1, by its
numbers, what [ocsp_response::status](x509-ocsp_response/status.md) gives. Go's `ocsp.ResponseStatus`. A response that
is not successful holds nothing else, and does not verify.

| Value | Description |
|---|---|
| `successful` | a response with the status of the certificates asked |
| `malformed_request` | the request did not parse |
| `internal_error` | the responder is in an inconsistent state |
| `try_later` | the responder cannot answer now |
| `sig_required` | the responder wants a signed request |
| `unauthorized` | the responder does not answer for this certificate (it knows nothing of its issuer) |

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    unsigned char unauthorized[] = {0x30, 0x03, 0x0a, 0x01, 0x06};
    auto response = crypto::x509::ocsp_response::parse(
        slice<const byte>(reinterpret_cast<const byte*>(unauthorized), 5));
    println("{}", response->status() == crypto::x509::ocsp_response_status::unauthorized);
}
```

Output:

```text
true
```

## See also

- [ocsp_response::status](x509-ocsp_response/status.md): the status of a response
- [sgcl::crypto::x509](x509.md)
