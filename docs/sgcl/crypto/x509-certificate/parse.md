[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [certificate](../x509-certificate.md)

# sgcl::crypto::x509::certificate::parse

```cpp
[[nodiscard]] static expected<certificate, error> parse(const slice<const byte>& der) noexcept;
```

Reads a certificate from its DER. Anything that is not a certificate in **strict DER** is refused: lengths in their
shortest form, no indefinite length, no string cut in pieces, a BOOLEAN of `00` or `FF`, integers and OIDs in their
shortest form, nothing after the last field of a structure (a TBSCertificate, an extension, a name's attribute), the
times only as `YYMMDDHHMMSSZ` (UTCTime, 1950 to 2049) and `YYYYMMDDHHMMSSZ` (GeneralizedTime) with every field in
range, a version 1 or 2 certificate without extensions, no extension twice. A certificate this function reads is one
OpenSSL reads too (the fuzzer holds it to that); a few that Go and OpenSSL read are refused as not DER.

What a certificate may hold is bounded (§11 A7 of the design): at most 128 KiB, checked before anything is copied, 64
extensions, 1024 names in its SAN, 256 subtrees of name constraints, 64 extended key usages, 64 policies, 64
attributes in a name, sixteen levels of nesting. Real certificates stay far below.

A public key of an algorithm the module has no type for (DSA, X25519, P-521, ML-DSA), one its type refuses (an RSA key
under 1024 bits, a point off the curve), or an EC key whose parameters are explicit or NULL rather than a named curve
leaves the certificate readable and its key `key_kind::none`, as Go leaves a key of an unknown algorithm; a key whose
DER is broken is a certificate that cannot be read.

## Parameters

| Parameter | Description |
|---|---|
| `der` | the bytes of the certificate |

## Return value

The certificate, or a [crypto::error](../error.md) `errc::malformed` with the offset of the byte where the reading
stopped, for anything that is not a certificate, is not strict DER, or passes a bound.

## Complexity

Linear in the length of `der`.

## Exceptions

None.

## Notes

Where the reading differs from DER, it is as Go and OpenSSL read: three values DER wants left out as their DEFAULT are
read when written — `version [0]` of v1, an extension's `critical` FALSE, `basicConstraints`' cA FALSE — and so is a
`keyUsage` BIT STRING with trailing zero bits. A SubjectPublicKeyInfo whose BIT STRING has unused bits is `malformed`
for a key of the module's algorithms, and read with `key_kind::none` for any other. A critical
`subjectKeyIdentifier`, `authorityKeyIdentifier` or `authorityInfoAccess` is `malformed`: RFC 5280 has them
non-critical.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // a certificate of the tree, its PEM read as a block of bytes
    encoding::pem block(io::read_text("tests/net/tls_testdata/rsa.pem"));
    auto cert = crypto::x509::certificate::parse(block.bytes());
    println("{}", cert->subject().to_string());

    auto cut = crypto::x509::certificate::parse(block.bytes().as_slice().subslice(0, 100));
    println("{}", cut.error().message());
}
```

Output:

```text
CN=localhost
sgcl::crypto::x509: not a Certificate SEQUENCE
```

## See also

- [from_pem](from_pem.md): the certificate of a PEM text
- [raw](raw.md): the bytes it was read from
- [sgcl::crypto::x509::certificate](../x509-certificate.md)
