[sgcl](../../README.md) › [crypto](../README.md) › [x25519](../x25519.md) › [public_key](../x25519-public_key.md)

# sgcl::crypto::x25519::public_key::from_bytes

```cpp
static expected<public_key, error> from_bytes(const slice<const byte>& bytes) noexcept;
```

Makes the key of 32 bytes, as the peer sent them. Any 32 bytes are a key, as RFC 7748 §5 and Go have it: bit 255
is ignored and a value of p = 2²⁵⁵ − 19 or more is reduced when the key is used. A key of small order is taken here,
and refused by [shared_secret](../x25519-private_key/shared_secret.md), whose result it would make zero.

## Parameters

| Parameter | Description |
|---|---|
| `bytes` | the 32 bytes of the key |

## Return value

The key, or an [error](../error.md) `errc::invalid_key` when `bytes` is not 32 bytes long.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // RFC 7748 §6.1: Alice's public key
    auto alice = crypto::x25519::public_key::from_bytes(
        encoding::hex::decode("8520f0098930a754748b7ddcb43ef75a0dbf3a0d26381af4eba4a98eaa9b4e6a"));
    println("{}", alice.has_value());

    // the all-zero key, of small order: taken here, refused by shared_secret
    auto zero = crypto::x25519::public_key::from_bytes(array<byte, 32>{});
    println("{}", zero.has_value());

    auto short_key = crypto::x25519::public_key::from_bytes(encoding::hex::decode("8520f009"));
    println("{}", short_key.error().message());
}
```

Output:

```text
true
true
an X25519 public key is 32 bytes
```

## See also

- [from_pkix_der](from_pkix_der.md): the key of a SubjectPublicKeyInfo
- [bytes](bytes.md): the reverse
- [sgcl::crypto::x25519::public_key](../x25519-public_key.md)
