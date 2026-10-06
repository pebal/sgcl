[sgcl](../../../README.md) › [net](../../README.md) › [tls](../README.md) › [identity](README.md)

# sgcl::net::tls::identity::from_pem

```cpp
static expected<identity, io::error> from_pem(const string& certificate_chain_pem,
                                              const slice<const byte>& key_pem) noexcept;
```

Makes an identity of a certificate chain and its private key, each in PEM, and returns what it refuses as an error:
Go's `tls.X509KeyPair`. The chain is every `CERTIFICATE` block of the first text, the leaf first; blocks of other
labels are passed over. The key is the first private key block of the second: `PRIVATE KEY` (PKCS #8: Ed25519,
ECDSA P-256, P-384 or P-521, RSA), `EC PRIVATE KEY` (SEC 1: P-256, P-384 or P-521) or `RSA PRIVATE KEY` (PKCS #1). Text around
it and blocks of other labels are passed over, and it has to be a key TLS 1.3 signs with. An encrypted key
(`ENCRYPTED PRIVATE KEY`, or the old headers `Proc-Type` and `DEK-Info`) is refused: decrypt it first. A key that
is not the leaf's, checked by a signature verified under the leaf's public key, is refused.

The key's PEM is read where it lies and its DER goes straight into a
[secret_bytes](../../../crypto/secret_bytes/README.md), then into the key's unmanaged block: given the bytes of
[crypto::read_secret](../../../crypto/secret/README.md), the key never passes through managed memory. A `string` converts
too, but its bytes are managed; that is the caller's choice.

## Parameters

| Parameter | Description |
|---|---|
| `certificate_chain_pem` | the chain in PEM, the leaf first |
| `key_pem` | the leaf's private key in PEM |

## Return value

The identity. Or the [io::error](../../../io/error/README.md), its operation `identity`:

- `crypto::errc::malformed` for a chain that is not PEM, a certificate that does not parse, no certificate, no
  private key block, a key of a kind TLS 1.3 does not sign with, a key that is not the leaf's;
- `crypto::errc::unsupported` for an encrypted key.

## Complexity

Linear in the size of the PEM, and one signature by the key verified under the leaf's public key.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"
#include "sgcl/net/tls.h"

using namespace sgcl;

int main() {
    string leaf_pem = io::read_text("tests/net/tls_testdata/rsa.pem");
    auto made = net::tls::identity::from_pem(leaf_pem,
                                             crypto::read_secret("tests/net/tls_testdata/rsa.key"));
    println("{}", made.has_value());

    auto mismatched = net::tls::identity::from_pem(
        leaf_pem, crypto::read_secret("tests/net/tls_testdata/ecdsa.key"));
    println("{}", mismatched.error().message());

    auto bare = net::tls::identity::from_pem("no certificate here",
                                             crypto::read_secret("tests/net/tls_testdata/rsa.key"));
    println("{}", bare.error().code() == crypto::errc::malformed);

    string encrypted = "-----BEGIN ENCRYPTED PRIVATE KEY-----\n"
                       "MIIBvTBXBgkqhkiG9w0BBQ0w\n"
                       "-----END ENCRYPTED PRIVATE KEY-----\n";
    auto locked = net::tls::identity::from_pem(leaf_pem, encrypted);
    println("{}", locked.error().code() == crypto::errc::unsupported);
}
```

Output:

```text
true
identity the private key is not the leaf certificate's: malformed data
true
true
```

## See also

- [(constructor)](identity.md): the same, an error thrown
- [certificates](certificates.md): the chain read
- [crypto::x509](../../../crypto/x509.md): the certificates
- [sgcl::net::tls::identity](README.md)
