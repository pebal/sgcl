[sgcl](../../README.md) › [crypto](../README.md) › [x25519](../x25519.md) › [private_key](../x25519-private_key.md)

# sgcl::crypto::x25519::private_key::from_bytes

```cpp
static expected<private_key, error> from_bytes(const slice<const byte>& bytes) noexcept;
```

Makes the key of 32 bytes, and computes its public key. Any 32 bytes are a key: they are clamped (the low three bits
cleared, bit 255 cleared, bit 254 set) each time they are used, not here, so [bytes](bytes.md) gives back what was
given, as Go's does; two keys whose bytes differ only in the bits clamping sets have the same public key and the
same secrets, and are not equal by [==](operator_cmp.md).

## Parameters

| Parameter | Description |
|---|---|
| `bytes` | the 32 bytes of the key |

## Return value

The key, or an [error](../error.md) `errc::invalid_key` when `bytes` is not 32 bytes long.

## Complexity

Constant: one fixed-base multiplication.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // RFC 7748 §6.1's private key of Alice, and the same with the low three bits flipped
    vector<byte> bytes =
        encoding::hex::decode("77076d0a7318a57d3c16c17251b26645df4c2f87ebc0992ab177fba51db92c2a");
    auto alice = crypto::x25519::private_key::from_bytes(bytes);
    println("{}", encoding::hex::encode(alice->public_key().bytes()));

    bytes[0] ^= byte{7};
    auto flipped = crypto::x25519::private_key::from_bytes(bytes);
    println("{} {}", flipped->public_key() == alice->public_key(), flipped == alice);

    auto short_key = crypto::x25519::private_key::from_bytes(bytes.as_slice().subslice(1));
    println("{}", short_key.error().message());
}
```

Output:

```text
8520f0098930a754748b7ddcb43ef75a0dbf3a0d26381af4eba4a98eaa9b4e6a
true false
an X25519 private key is 32 bytes
```

## See also

- [generate](generate.md): a new key
- [bytes](bytes.md): the reverse
- [sgcl::crypto::x25519::private_key](../x25519-private_key.md)
