[sgcl](../README.md) › [crypto](README.md) › [x509](x509.md)

# sgcl::crypto::x509::ocsp_verify_options

```cpp
#include "sgcl/crypto/x509_revocation.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto::x509 {
    struct ocsp_verify_options {
        optional<time::datetime> time;
        duration skew = 5 * minute;
        duration max_age = 7 * 24 * hour;
        vector<byte> nonce;
    };
}
```

**Requires [rooted](../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::crypto::x509::ocsp_verify_options` is what [ocsp_response::verify](x509-ocsp_response/verify.md) checks the
times and the nonce of a response against. Every field has a default; a call names what differs:
`response.verify(cert, issuer, {.nonce = request.nonce()})`.

## Member objects

| Member | Description |
|---|---|
| `time` | the instant the response must be current at; `nullopt`, the default: `time::now()` |
| `skew` | how far the clocks of the responder and of this machine may differ: a thisUpdate up to this far ahead and a nextUpdate up to this far behind are taken; 5 minutes by default |
| `max_age` | how long after its thisUpdate a response without a nextUpdate is current (RFC 6960 §4.2.2.1: such a response says the status is always fresh at the responder); 7 days by default |
| `nonce` | the request's nonce the response must carry ([ocsp_request::nonce](x509-ocsp_request/nonce.md)); empty, the default: not asked for |

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"
#include "sgcl/time.h"

using namespace sgcl;

int main() {
    string dir = "tests/crypto/data/revocation/";
    auto leaf = crypto::x509::certificate::from_pem(io::read_text(dir + "good.pem")).value();
    auto issuer = crypto::x509::certificate::from_pem(io::read_text(dir + "int.pem")).value();
    // a response the issuer signed itself
    auto response = crypto::x509::ocsp_response::parse(
        io::read_file(dir + "ocsp_good_issuer.der").value()).value();

    auto issued = response.responses()[0].this_update;
    crypto::x509::ocsp_verify_options early;
    early.time = issued - 4 * minute;   // within the skew
    println("{}", response.verify(leaf, issuer, early).has_value());
    early.skew = minute;
    auto refused = response.verify(leaf, issuer, early);
    println("{}", refused.error().reason() == crypto::x509::reason::not_yet_valid);
}
```

Output:

```text
true
true
```

## See also

- [ocsp_response::verify](x509-ocsp_response/verify.md): what takes them
- [sgcl::crypto::x509](x509.md)
