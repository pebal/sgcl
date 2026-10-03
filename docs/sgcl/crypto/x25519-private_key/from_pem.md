[sgcl](../../README.md) › [crypto](../README.md) › [x25519](../x25519.md) › [private_key](README.md)

# sgcl::crypto::x25519::private_key::from_pem

```cpp
static expected<private_key, error> from_pem(const slice<const byte>& text) noexcept;
```

Makes the key of PEM text: the first private key's block, `PRIVATE KEY` over a PKCS #8, its base64 decoded straight
into a [secret_bytes](../secret_bytes/README.md) and read as [from_pkcs8_der](from_pkcs8_der.md) reads it. Text before,
between and after blocks is passed over, as RFC 7468 §5.2 lets it be, and so are blocks of other labels. The text
is a file's bytes, [read_secret](../read_secret.md)'s, or the program's: `from_pem(crypto::read_secret(path))`.
[encoding::pem](../../encoding/pem/README.md) is not the way for a private key: it decodes into a managed vector, where a
private key must not be.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the PEM text |

## Return value

The key, or an [error](../error/README.md): `errc::malformed` for text with no private key's block, a block whose base64
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
    // RFC 7748 §6.1's private key of Alice, as OpenSSL writes it
    auto alice = crypto::x25519::private_key::from_pem(
        "Alice's key\n"
        "-----BEGIN PRIVATE KEY-----\n"
        "MC4CAQAwBQYDK2VuBCIEIHcHbQpzGKV9PBbBclGyZkXfTC+H68CZKrF3+6UduSwq\n"
        "-----END PRIVATE KEY-----\n");
    println("{}", encoding::hex::encode(alice->public_key().bytes()));

    // RFC 8032's TEST 1 key, of Ed25519
    auto other = crypto::x25519::private_key::from_pem(
        "-----BEGIN PRIVATE KEY-----\n"
        "MC4CAQAwBQYDK2VwBCIEIJ1hsZ3v/VpguoRK9JLsLMREScVpezJpGXA7rAMcrn9g\n"
        "-----END PRIVATE KEY-----\n");
    println("{}", other.error().message());

    println("{}", crypto::x25519::private_key::from_pem("no key here").error().message());
}
```

Output:

```text
8520f0098930a754748b7ddcb43ef75a0dbf3a0d26381af4eba4a98eaa9b4e6a
offset 9: the key is of another algorithm
PEM: no private key block
```

## See also

- [to_pem](to_pem.md): the reverse
- [read_secret](../read_secret.md): a key file read as a secret
- [sgcl::crypto::x25519::private_key](README.md)
