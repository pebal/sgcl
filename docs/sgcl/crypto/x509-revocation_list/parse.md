[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [revocation_list](README.md)

# sgcl::crypto::x509::revocation_list::parse

```cpp
static expected<revocation_list, error> parse(const slice<const byte>& der) noexcept;
```

Reads a CRL in DER (RFC 5280 §5): its issuer, its times, its extensions (cRLNumber, deltaCRLIndicator,
authorityKeyIdentifier, issuingDistributionPoint; others kept as they are) and its entries, Go's
`x509.ParseRevocationList`. Strict DER. The entries are not copied out: each is an index into the list's bytes (its
serial number, its time, its reason, its invalidity date), so a list of a hundred thousand is read with one
allocation for the index. Nothing is verified: [check_signature_from](check_signature_from.md) and
[status_of](status_of.md) do that.

## Parameters

| Parameter | Description |
|---|---|
| `der` | the CRL's DER, as a distribution point serves it (`application/pkix-crl`) |

## Return value

The list, or `errc::malformed` with the offset for anything that is not one or is not strict DER (a list of 4 GiB or more among them).

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
    auto issuer = crypto::x509::certificate::from_pem(io::read_text(dir + "int.pem")).value();
    auto crl = crypto::x509::revocation_list::parse(io::read_file(dir + "int.crl").value()).value();
    println("{} {}", crl.issuer().to_string(), crl.size());
    println("{}", crypto::x509::revocation_list::parse(
        io::read_file(dir + "int.pem").value()).error().message());
}
```

Output:

```text
CN=SGCL Revocation Intermediate 2
sgcl::crypto::x509: not a CertificateList SEQUENCE
```

## See also

- [from_pem](from_pem.md): a list in PEM
- [status_of](status_of.md): the status of a certificate by it
- [sgcl::crypto::x509::revocation_list](README.md)
