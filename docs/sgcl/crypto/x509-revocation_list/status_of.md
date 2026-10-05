[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [revocation_list](README.md)

# sgcl::crypto::x509::revocation_list::status_of

```cpp
expected<revocation_status, error> status_of(const certificate& cert,                                   // (1)
                                            const certificate& issuer,
                                            optional<time::datetime> time = nullopt) const noexcept;
expected<revocation_status, error> status_of(const certificate& cert,                                   // (2)
                                            const certificate& issuer,
                                            const revocation_list& delta,
                                            optional<time::datetime> time = nullopt) const noexcept;
```

Returns the status of the certificate by the list (RFC 5280 §6.3):

1. By this complete list: signed by `issuer` ([check_signature_from](check_signature_from.md)), the list of the
   certificate's issuer, current at the time (its thisUpdate no later, its nextUpdate no earlier), of a scope that
   covers the certificate (its issuingDistributionPoint: user or CA certificates as the certificate is, its point one
   of the certificate's distribution points, not indirect), no critical extension unknown. `revoked` when it lists
   the certificate (an entry of `certificateHold` included), else `good`; a list of only some reasons that does not
   list it cannot say.
2. By this complete list and a delta of it (RFC 5280 §5.2.4): both checked as (1), the delta of the same issuer and
   scope and of a base number no greater than this list's number. An entry of the delta decides — `removeFromCRL`
   makes a certificate on hold good again — else this list's.

## Parameters

| Parameter | Description |
|---|---|
| `cert` | the certificate |
| `issuer` | its issuer, the list's |
| `delta` | a delta CRL of this list |
| `time` | the instant the list must be current at; `nullopt`: now |

## Return value

`good` or `revoked`, or `errc::verification` with the reason when the list cannot say: `revocation_unknown` (a delta
alone, a list of another issuer or of a scope that does not cover the certificate, an indirect list, a delta of a
later list), `expired`, `not_yet_valid`, `unhandled_critical_extension`, or what
[check_signature_from](check_signature_from.md) gives.

## Complexity

The verification of the signatures, and a scan of the entries.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string dir = "tests/crypto/data/revocation/";
    auto issuer = crypto::x509::certificate::from_pem(io::read_text(dir + "int.pem")).value();
    auto crl = crypto::x509::revocation_list::parse(io::read_file(dir + "int.crl").value()).value();
    auto good = crypto::x509::certificate::from_pem(io::read_text(dir + "good.pem")).value();
    auto held = crypto::x509::certificate::from_pem(io::read_text(dir + "held.pem")).value();
    println("{}", crl.status_of(good, issuer) == crypto::x509::revocation_status::good);
    println("{}", crl.status_of(held, issuer) == crypto::x509::revocation_status::revoked);

    // the delta takes the certificate off hold
    auto delta = crypto::x509::revocation_list::parse(
        io::read_file(dir + "int_delta.crl").value()).value();
    println("{}", crl.status_of(held, issuer, delta) == crypto::x509::revocation_status::good);
    println("{}", delta.status_of(held, issuer).error().message());
}
```

Output:

```text
true
true
true
sgcl::crypto::x509: a delta CRL says nothing alone: status_of(cert, issuer, delta) of its complete list
```

## See also

- [lookup](lookup.md): an entry, without the checks
- [net::tls::config](../../net/tls/config.md): `crls`, checked by a connection
- [sgcl::crypto::x509::revocation_list](README.md)
