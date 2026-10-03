[sgcl](../../README.md) › [crypto](../README.md) › [p256](../p256.md) › [ecdh_key](../p256-ecdh_key.md)

# sgcl::crypto::p256::ecdh_key::shared_secret

```cpp
expected<secret<size>, error> shared_secret(const p256::public_key& peer) const;
```

The secret this key shares with `peer`, Go's `ecdh.PrivateKey.ECDH`: the x-coordinate of d·Q, `size` bytes
big-endian (SEC 1 §3.3.1, what TLS and Go give). The peer computes the same secret from its own key and this key's
public one.

The secret is not a key: it is put through a key derivation, [hkdf](../hkdf.md) with the protocol's salt and info,
before it keys anything. The peer is a [public_key](../p256-public_key.md), so it was checked when it was made: on the
curve and not the identity.

`p384::ecdh_key::shared_secret` takes a `p384::public_key` and gives a `secret<48>`.

## Parameters

| Parameter | Description |
|---|---|
| `peer` | the other side's public key |

## Return value

The shared secret, a [secret\<32\>](../secret.md), or a [crypto::error](../error.md) with `errc::invalid_key` if the
product were the identity, which a key in range and a point of the curve do not allow on a curve of prime order.

## Complexity

Constant: one multiplication of a point by the secret scalar.

## Exceptions

`std::logic_error` when the key was moved from.

## Notes

The multiplication runs in constant time, and the scalar's bytes and the product are zeroed before the function
returns.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // NIST's first test of ECC CDH on P-256: the scalar dIUT, the peer's point QCAVS
    // and the shared secret ZIUT they give
    auto key = crypto::p256::ecdh_key::from_bytes(encoding::hex::decode(
        "7d7dc5f71eb29ddaf80d6214632eeae03d9058af1fb6d22ed80badb62bc1a534"));
    auto peer = crypto::p256::public_key::from_bytes(encoding::hex::decode(
        "04700c48f77f56584c5cc632ca65640db91b6bacce3a4df6b42ce7cc838833d287"
        "db71e509e3fd9b060ddb20ba5c51dcc5948d46fbf640dfe0441782cab85fa4ac"));
    auto expected_secret = encoding::hex::decode(
        "46fc62106420ff012e54a434fbdd2d25ccc5852060561e68040dd7778997bd7b");

    auto shared = key->shared_secret(peer);
    println("{}", crypto::constant_time::equal(shared->bytes(), expected_secret));

    // a key for a cipher, derived from the secret
    auto session = crypto::hkdf_sha256::derive("salt", shared, "session key", 32);
    println("{} bytes", session.size());
}
```

Output:

```text
true
32 bytes
```

## See also

- [public_key](public_key.md): what the peer receives from this key
- [hkdf](../hkdf.md): the key derived from the secret
- [secret](../secret.md): the form of the secret
- [sgcl::crypto::p256::ecdh_key](../p256-ecdh_key.md)
