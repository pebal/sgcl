[sgcl](../../README.md) › [crypto](../README.md) › [p256](../p256.md) › [ecdh_key](README.md)

# sgcl::crypto::p256::ecdh_key::from_pem

```cpp
static expected<ecdh_key, error> from_pem(const slice<const byte>& text) noexcept;
```

Reads the key from PEM text (RFC 7468): the first private key block of the text, which must be `PRIVATE KEY`
(PKCS #8, read as [from_pkcs8_der](from_pkcs8_der.md) reads it). Its base64 is decoded straight into a
[secret_bytes](../secret_bytes/README.md): [encoding::pem](../../encoding/pem/README.md) would put the DER in managed memory. Text
before, between and after the blocks is passed over, and so are blocks of other labels. For a key file:
`from_pem(crypto::read_secret(path))`.

`p384::ecdh_key::from_pem` reads the keys of P-384.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the PEM text: a file's bytes, [read_secret](../read_secret.md)'s or the program's |

## Return value

The key, or a [crypto::error](../error/README.md):

- `errc::malformed` for text with no private key block, a block without its end, base64 that does not decode, or a
  first private key block of another type (`EC PRIVATE KEY`, `RSA PRIVATE KEY`);
- `errc::unsupported` for an encrypted key, PKCS #8's `ENCRYPTED PRIVATE KEY` or a block with RFC 1421 headers
  (`Proc-Type`, `DEK-Info`);
- the errors of `from_pkcs8_der` for the DER inside.

## Complexity

Linear in the size of `text`, and one multiplication of the base point.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto key = crypto::p256::ecdh_key::generate();
    auto read = crypto::p256::ecdh_key::from_pem(key.to_pem());
    println("{}", read->public_key() == key.public_key());

    // an ECDSA key in PKCS #8 is a PRIVATE KEY too
    auto signer = crypto::p256::private_key::generate();
    println("{}", crypto::p256::ecdh_key::from_pem(signer.to_pem()).has_value());

    auto none = crypto::p256::ecdh_key::from_pem(string("no key here"));
    println("{}", none.error().message());
}
```

Output:

```text
true
true
sgcl::crypto::p256: PEM: no private key block
```

## See also

- [to_pem](to_pem.md): the key as PEM
- [from_pkcs8_der](from_pkcs8_der.md): the DER inside
- [read_secret](../read_secret.md): a key file read into a `secret_bytes`
- [sgcl::crypto::p256::ecdh_key](README.md)
