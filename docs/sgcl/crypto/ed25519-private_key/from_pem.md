[sgcl](../../README.md) › [crypto](../README.md) › [ed25519](../ed25519.md) › [private_key](../ed25519-private_key.md)

# sgcl::crypto::ed25519::private_key::from_pem

```cpp
static expected<private_key, error> from_pem(const slice<const byte>& text) noexcept;
```

Makes the key of PEM text: the first private key's block, `PRIVATE KEY` over a PKCS #8, its base64 decoded straight
into a [secret_bytes](../secret_bytes.md) and read as [from_pkcs8_der](from_pkcs8_der.md) reads it. Text before,
between and after blocks is passed over, as RFC 7468 §5.2 lets it be, and so are blocks of other labels. The text
is a file's bytes, [read_secret](../read_secret.md)'s, or the program's: `from_pem(crypto::read_secret(path))`.
[encoding::pem](../../encoding/pem.md) is not the way for a private key: it decodes into a managed vector, where a
private key must not be.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the PEM text |

## Return value

The key, or an [error](../error.md): `errc::malformed` for text with no private key's block, a block whose base64
does not decode, or one of another key's type (`EC PRIVATE KEY`, `RSA PRIVATE KEY`); `errc::unsupported` for an
encrypted key, PKCS #8's `ENCRYPTED PRIVATE KEY` or one with RFC 1421's headers, and for a key of another
algorithm; and the errors of [from_pkcs8_der](from_pkcs8_der.md).

## Complexity

Linear in the size of `text`.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // RFC 8032's TEST 1 key, as OpenSSL writes it
    auto key = crypto::ed25519::private_key::from_pem(
        "-----BEGIN PRIVATE KEY-----\n"
        "MC4CAQAwBQYDK2VwBCIEIJ1hsZ3v/VpguoRK9JLsLMREScVpezJpGXA7rAMcrn9g\n"
        "-----END PRIVATE KEY-----\n");
    println("{}", encoding::hex::encode(key->public_key().bytes()));

    auto encrypted = crypto::ed25519::private_key::from_pem(
        "-----BEGIN ENCRYPTED PRIVATE KEY-----\n"
        "MC4CAQAw\n"
        "-----END ENCRYPTED PRIVATE KEY-----\n");
    println("{}", encrypted.error().message());

    auto ec = crypto::ed25519::private_key::from_pem(
        "-----BEGIN EC PRIVATE KEY-----\n"
        "MC4CAQAw\n"
        "-----END EC PRIVATE KEY-----\n");
    println("{}", ec.error().message());
}
```

Output:

```text
d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a
PEM: an encrypted private key (PKCS #8 EncryptedPrivateKeyInfo)
PEM: a block of another key's type
```

## See also

- [to_pem](to_pem.md): the reverse
- [read_secret](../read_secret.md): a key file read as a secret
- [sgcl::crypto::ed25519::private_key](../ed25519-private_key.md)
