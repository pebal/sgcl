[sgcl](../../README.md) › [crypto](../README.md) › [rsa](../rsa.md) › [private_key](README.md)

# sgcl::crypto::rsa::private_key::from_pem

```cpp
static expected<private_key, error> from_pem(const slice<const byte>& text) noexcept;
```

Reads the key from PEM text: the first private key block, `PRIVATE KEY` (PKCS #8, read by
[from_pkcs8_der](from_pkcs8_der.md)) or `RSA PRIVATE KEY` (PKCS #1, read by [from_pkcs1_der](from_pkcs1_der.md)), its
base64 decoded straight into a [secret_bytes](../secret_bytes/README.md): [encoding::pem](../../encoding/pem/README.md) would put
the DER in managed memory. Text around the block is passed over. A key file is read with
[read_secret](../read_secret.md), `from_pem(crypto::read_secret(path))`, so that its bytes never pass through managed
memory either.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the PEM text: a file's bytes, `read_secret`'s or the program's |

## Return value

The key, or a [crypto::error](../error/README.md): `errc::malformed` for a text with no private key block or a block of
another key's type, `errc::unsupported` for an encrypted key, and the errors of the reader of the block.

## Complexity

Linear in the length of the text, then that of the reader of the block.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto pem = crypto::read_secret("tests/net/tls_testdata/rsa.key");  // the tree's test key
    auto key = crypto::rsa::private_key::from_pem(pem);
    println("{} bits", key->bits());
    println("{}", crypto::rsa::private_key::from_pem("no key here").error().message());
}
```

Output:

```text
2048 bits
sgcl::crypto::rsa: PEM: no private key block
```

## See also

- [to_pem](to_pem.md): writes the PEM
- [read_secret](../read_secret.md): a key file into a `secret_bytes`
- [sgcl::crypto::rsa::private_key](README.md)
