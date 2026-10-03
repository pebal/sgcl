[sgcl](../../README.md) › [crypto](../README.md) › [p256](../p256.md) › [ecdh_key](../p256-ecdh_key.md)

# sgcl::crypto::p256::ecdh_key::to_pem

```cpp
secret_bytes to_pem() const;
```

The key as PEM text (RFC 7468): a `PRIVATE KEY` block over its PKCS #8 ([to_pkcs8_der](to_pkcs8_der.md)), the base64
in lines of 64 characters, byte for byte as Go's `pem.Encode` of `x509.MarshalPKCS8PrivateKey` and OpenSSL's
`genpkey` write it. [from_pem](from_pem.md) reads it back.

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
    // the published scalar dIUT of NIST's first test of ECC CDH on P-256;
    // a key of a program's own is never printed
    auto key = crypto::p256::ecdh_key::from_bytes(encoding::hex::decode(
        "7d7dc5f71eb29ddaf80d6214632eeae03d9058af1fb6d22ed80badb62bc1a534"));
    io::stdout.write(key->to_pem());
}
```

Output:

```text
-----BEGIN PRIVATE KEY-----
MIGHAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBG0wawIBAQQgfX3F9x6yndr4DWIU
Yy7q4D2QWK8fttIu2AuttivBpTShRANCAATq0hhZARnoh2spFG/4nKYXcMTtu/l9
OM44XtKB2KayMCivYSgf014vpwAlI6zIWkKcsG7mZIMlOJ9Z7fzhQFFB
-----END PRIVATE KEY-----
```

## See also

- [from_pem](from_pem.md): the key of PEM text
- [to_pkcs8_der](to_pkcs8_der.md): the DER inside
- [sgcl::crypto::p256::ecdh_key](../p256-ecdh_key.md)
