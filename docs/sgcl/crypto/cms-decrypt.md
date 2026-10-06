[sgcl](../README.md) › [crypto](README.md) › [cms](cms.md)

# sgcl::crypto::cms::decrypt

```cpp
expected<vector<byte>, error> decrypt(const slice<const byte>& enveloped, const x509::certificate& recipient,
                                      const x509::signing_key& key) noexcept;
```

The content of an EnvelopedData or AuthEnvelopedData (DER or BER) for the recipient whose certificate and key are
given: the recipient's entry found by its issuer and serial number (or subject key identifier), the content key
unwrapped (RSAES-OAEP of SHA-1 to SHA-512; ECDH with the X9.63 KDF of SHA-1 to SHA-512 and AES key wrap) into plain
memory, the content decrypted (AES-GCM, AES-CBC, of 128, 192 or 256 bits). An RSA transport that does not decrypt
goes on with a random key, so that it fails as a wrong tag fails (RFC 3218 §2.3.2). The key, through
[x509::signing_key](x509-signing_key/README.md), is a typed key or a PKCS #12 file's.

## Parameters

| Parameter | Description |
|---|---|
| `enveloped` | the ContentInfo |
| `recipient` | the recipient's certificate |
| `key` | its private key |

## Return value

The content, or an error: `errc::invalid_key` for no recipient of the certificate or a key of another kind;
`errc::authentication` for a content that does not decrypt or authenticate; `errc::unsupported` for what the
module does not have, named — RSA PKCS #1 v1.5 key transport (OpenSSL's default for RSA: its padding oracle), 3DES,
RC2; `errc::malformed` for bytes that do not read.

## Complexity

Linear in the length of the content, and one key transport or agreement.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/crypto/smime.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // a CA and a leaf of S/MIME it issued (a program reads its own from files or a .p12)
    auto ca_key = crypto::p256::private_key::generate();
    crypto::x509::certificate_template ct;
    ct.common_name = "Example CA";
    ct.is_ca = true;
    auto ca = crypto::x509::create_certificate(ct, ca_key);
    auto key = crypto::p256::private_key::generate();
    crypto::x509::certificate_template lt;
    lt.common_name = "alice";
    lt.email_addresses = {"alice@example.test"};
    lt.ext_key_usages = {crypto::x509::ext_key_usage::email_protection};
    auto alice = crypto::x509::create_certificate(lt, key.public_key().to_pkix_der(), ca, ca_key);
    crypto::cms::verify_options trust;
    trust.chain.roots = crypto::x509::certificate_pool();
    trust.chain.roots->add(ca);
    crypto::x509::chain to;
    to.push_back(alice);
    auto sealed = crypto::cms::encrypt("for alice only", to);
    auto other = crypto::p256::private_key::generate();
    println("{}", crypto::cms::decrypt(sealed, alice, other).error().message());
}
```

Output:

```text
sgcl::crypto::cms: the content key does not unwrap
```

## See also

- [encrypt](cms-encrypt.md)
- [sgcl::crypto::cms](cms.md)
