[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [signing_key](README.md)

# sgcl::crypto::x509::signing_key::signing_key

```cpp
signing_key(const p256::private_key& k) noexcept;       // (1)
signing_key(const p384::private_key& k) noexcept;       // (2)
signing_key(const ed25519::private_key& k) noexcept;    // (3)
signing_key(const rsa::private_key& k) noexcept;        // (4)
```

A view of the key `k`, implicit: a function that takes a `signing_key` takes the key itself.

1. A P-256 key: ECDSA over SHA-256.
2. A P-384 key: ECDSA over SHA-384.
3. An Ed25519 key.
4. An RSA key: PKCS #1 v1.5 over SHA-256.

## Parameters

| Parameter | Description |
|---|---|
| `k` | the private key, which must outlive the view |

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto ec = crypto::p256::private_key::generate();
    auto ed = crypto::ed25519::private_key::generate();
    crypto::x509::signing_key a = ec;
    crypto::x509::signing_key b = ed;
    using crypto::x509::key_kind;
    println("{} {}", a.kind() == key_kind::p256, b.kind() == key_kind::ed25519);
}
```

Output:

```text
true true
```

## See also

- [kind](kind.md)
- [sgcl::crypto::x509::signing_key](README.md)
