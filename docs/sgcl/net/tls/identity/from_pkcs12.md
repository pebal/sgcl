[sgcl](../../../README.md) › [net](../../README.md) › [tls](../README.md) › [identity](README.md)

# sgcl::net::tls::identity::from_pkcs12

```cpp
static expected<identity, io::error> from_pkcs12(const slice<const byte>& file, const slice<const byte>& password) noexcept;
static expected<identity, io::error> from_pkcs12(const crypto::pkcs12& file) noexcept;
```

An identity of a PKCS #12 file (a `.p12`, a `.pfx`): its chain, the leaf first, and its key, which must be the leaf's.
The first reads the file's bytes under its password ([crypto::pkcs12::parse](../../../crypto/pkcs12/parse.md)), the
second takes a file read. What Go's `tls.X509KeyPair` is for PEM, for the files Windows and Java export; the key
goes from the file's plain memory into the identity's, never through managed memory.

## Parameters

| Parameter | Description |
|---|---|
| `file` | the file's bytes, or the file read |
| `password` | the file's password: bytes or text |

## Return value

The identity, or an [io::error](../../../io/error/README.md) of op `identity`: the errors of
[crypto::pkcs12::parse](../../../crypto/pkcs12/parse.md) (a wrong password `crypto::errc::authentication`);
`crypto::errc::malformed` for a file without a key or a certificate, a key of a kind TLS does not sign with, a key that
is not the leaf's.

## Complexity

Linear in the size of the file, besides its derivations, and one signature by the key verified under the leaf's
public key.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"
#include "sgcl/net/tls.h"

using namespace sgcl;

int main() {
    // a .p12 of the test certificate, made here (a server reads its own from a file)
    auto pem = crypto::x509::certificate::from_pem(io::read_text("tests/net/tls_testdata/ecdsa.pem"));
    auto key = crypto::p256::private_key::from_pem(crypto::read_secret("tests/net/tls_testdata/ecdsa.key"));
    crypto::x509::chain chain;
    chain.push_back(*pem);
    auto p12 = crypto::pkcs12::encode(*key, chain, "changeit");

    auto id = net::tls::identity::from_pkcs12(p12, "changeit");
    println("{} certificate(s)", id->certificates().size());
    auto wrong = net::tls::identity::from_pkcs12(p12, "nope");
    println("{}", wrong.error().message());
}
```

Output:

```text
1 certificate(s)
identity sgcl::crypto::pkcs12: the MAC does not verify (a wrong password?): message authentication failed
```

## See also

- [from_pem](from_pem.md): the same of PEM
- [crypto::pkcs12](../../../crypto/pkcs12/README.md)
- [sgcl::net::tls::identity](README.md)
