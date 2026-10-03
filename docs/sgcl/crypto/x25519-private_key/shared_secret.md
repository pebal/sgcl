[sgcl](../../README.md) › [crypto](../README.md) › [x25519](../x25519.md) › [private_key](README.md)

# sgcl::crypto::x25519::private_key::shared_secret

```cpp
expected<secret<32>, error> shared_secret(const x25519::public_key& peer) const;
```

Computes the secret shared with the owner of `peer`, X25519(private, peer) of RFC 7748 §5: the Montgomery ladder over
the 255 bits of the clamped scalar, which takes the same steps whatever the bits, its points swapped by masks, never
by a branch. A peer's key of small order makes the result all zeros whatever the private key; it is refused
(RFC 7748 §6.1), as OpenSSL and Go refuse it, found with no branch on the secret bytes. Go's
`PrivateKey.ECDH(remote)`.

The secret is not a key: it goes through a key derivation, [hkdf](../hkdf/README.md) with a label of the protocol, and what
comes out is the key of a cipher or a MAC, as TLS 1.3 and Noise do.

## Parameters

| Parameter | Description |
|---|---|
| `peer` | the other side's public key |

## Return value

The secret, a [secret\<32\>](../secret/README.md), or an [error](../error/README.md) `errc::invalid_key` when it would be all
zeros.

## Complexity

Constant: one ladder of 255 steps.

## Exceptions

`logic_error` when the key was moved from.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // RFC 7748 §5.2's first vector: a scalar, a u coordinate and their product
    auto key = crypto::x25519::private_key::from_bytes(
        encoding::hex::decode("a546e36bf0527c9d3b16154b82465edd62144c0ac1fc5a18506a2244ba449ac4"));
    auto peer = crypto::x25519::public_key::from_bytes(
        encoding::hex::decode("e6db6867583030db3594c1a424b15f7c726624ec26b3353b10a903a6d0ab1c4c"));
    auto published =
        encoding::hex::decode("c3da55379de9c6908e94ea4df28d084f32eccf03491c71f754b4075577a28552");

    auto shared = key->shared_secret(peer);
    println("{}", crypto::constant_time::equal(shared, published));

    // a key of small order: the all-zero one
    auto zero = crypto::x25519::public_key::from_bytes(array<byte, 32>{});
    println("{}", key->shared_secret(zero).error().message());
}
```

Output:

```text
true
the shared secret is zero: the peer's key is of small order
```

## See also

- [public_key](public_key.md): what the peer is sent
- [hkdf](../hkdf/README.md): what the secret goes through
- [sgcl::crypto::x25519::private_key](README.md)
