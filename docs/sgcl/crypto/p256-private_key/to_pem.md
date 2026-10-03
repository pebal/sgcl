[sgcl](../../README.md) › [crypto](../README.md) › [p256](../p256.md) › [private_key](../p256-private_key.md)

# sgcl::crypto::p256::private_key::to_pem

```cpp
secret_bytes to_pem() const;
```

The key as PEM text (RFC 7468): a `PRIVATE KEY` block over its PKCS #8 ([to_pkcs8_der](to_pkcs8_der.md)), the base64
in lines of 64 characters, byte for byte as Go's `pem.Encode` of `x509.MarshalPKCS8PrivateKey` and OpenSSL's
`genpkey` write it. [from_pem](from_pem.md) reads it back, and so does every program that reads a key file.

## Parameters

None.

## Return value

The text, in a [secret_bytes](../secret_bytes.md): it holds the secret scalar, so it is zeroed when it goes and never
lies in managed memory. It is written to a file as bytes, without passing through a `string`.

## Complexity

Constant.

## Exceptions

`std::logic_error` when the key was moved from.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // the published key of RFC 6979, A.2.5; a key of a program's own is never printed
    auto key = crypto::p256::private_key::from_bytes(encoding::hex::decode(
        "c9afa9d845ba75166b5c215767b1d6934e50c3db36e89b127b8a622b120f6721"));
    io::stdout.write(key->to_pem());
}
```

Output:

```text
-----BEGIN PRIVATE KEY-----
MIGHAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBG0wawIBAQQgya+p2EW6dRZrXCFX
Z7HWk05Qw9s26JsSe4piKxIPZyGhRANCAARg/tS6JVqdMclh63TGNW1owEm4kjth
+mzmaWIuYPKftnkD/hAIuLyZpBrp6VYovGTy8bIMLX6fUXejwpTURiKZ
-----END PRIVATE KEY-----
```

## See also

- [from_pem](from_pem.md): the key of PEM text
- [to_pkcs8_der](to_pkcs8_der.md): the DER inside
- [sgcl::crypto::p256::private_key](../p256-private_key.md)
