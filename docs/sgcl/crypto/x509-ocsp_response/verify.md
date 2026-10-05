[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [ocsp_response](README.md)

# sgcl::crypto::x509::ocsp_response::verify

```cpp
expected<ocsp_single_response, error> verify(const certificate& cert, const certificate& issuer,
                                             const ocsp_verify_options& o = {}) const noexcept;
```

Verifies the response for the certificate and gives its single response: Go's `ocsp.ParseResponseForCert` with the
checks of time and nonce it leaves to its caller (RFC 6960 §3.2). In order:

- the response successful;
- its signer the issuer itself, or a responder the issuer authorized: a certificate of the response, named by its
  responderID, that the issuer signed, with `id-kp-OCSPSigning` among its extended key usages, valid at the time
  (RFC 6960 §4.2.2.2);
- its signature valid under the signer's key;
- a single response whose CertID names the certificate (its serial number, the hashes of its issuer's name and key,
  by SHA-1 or SHA-2);
- current at the time: its thisUpdate no later than it, its nextUpdate (or, without one, its thisUpdate and
  `max_age`) no earlier, each by `skew`;
- the request's nonce echoed, when `o` gives one;
- no critical extension the module does not know.

The status is the responder's answer: a revoked certificate verifies, its single response saying `revoked`.

## Parameters

| Parameter | Description |
|---|---|
| `cert` | the certificate whose status is asked |
| `issuer` | its issuer |
| `o` | the time, the skew, the age of a response without nextUpdate, the nonce ([ocsp_verify_options](../x509-ocsp_verify_options.md)) |

## Return value

The single response of the certificate ([ocsp_single_response](../x509-ocsp_single_response.md)), or
`errc::verification` with the reason: `revocation_unknown` (a response not successful, no status of the
certificate, a nonce that differs), `unknown_authority` (a signer the issuer did not authorize), `incompatible_usage`
(a responder without `id-kp-OCSPSigning`), `invalid_signature`, `unsupported_algorithm`, `insecure_algorithm`,
`expired`, `not_yet_valid`, `unhandled_critical_extension`.

## Complexity

The verification of one or two signatures (the responder's certificate and the response), and a scan of the single responses.

## Exceptions

None.

## Notes

A delegated responder answers for its CA for days, with one certificate: its signature under the issuer is checked
once per process, and a responder found signed is remembered by the digests of its certificate and of the issuer's
key (the 32 last). The response's own signature is checked every time.

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
    auto response = crypto::x509::ocsp_response::parse(
        io::read_file(dir + "ocsp_good.der").value()).value();
    auto single = response.verify(leaf, issuer).value();
    println("{}", single.status == crypto::x509::revocation_status::good);

    auto revoked = crypto::x509::certificate::from_pem(io::read_text(dir + "revoked.pem")).value();
    println("{}", response.verify(revoked, issuer).error().message());

    auto later = time::datetime::from_unix(single.next_update->unix() + 3600, time::zone::utc());
    auto stale = response.verify(leaf, issuer, {.time = later});
    println("{}", stale.error().reason() == crypto::x509::reason::expired);
}
```

Output:

```text
true
sgcl::crypto::x509: the OCSP response holds no status of "CN=revoked.localhost"
true
```

## See also

- [parse](parse.md): the response read
- [check_signature_from](check_signature_from.md): the signature alone
- [net::tls::identity::set_ocsp_staple](../../net/tls/identity/set_ocsp_staple.md): a response a server staples
- [sgcl::crypto::x509::ocsp_response](README.md)
