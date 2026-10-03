[sgcl](../../README.md) › [crypto](../README.md) › [p256](../p256.md) › [private_key](README.md)

# sgcl::crypto::p256::private_key::to_ecdh

```cpp
p256::ecdh_key to_ecdh() const;
```

The same scalar as an [ecdh_key](../p256-ecdh_key/README.md), Go's `PrivateKey.ECDH`: for a protocol that needs a signature
and an agreement from one key. A key is used for one purpose, as in Go, so the ECDSA key does not agree and the ECDH
key does not sign; this is the way from the one to the other, and there is no way back. The new key is an object of
its own, zeroed when it goes. `p384::private_key::to_ecdh` gives a `p384::ecdh_key`.

## Parameters

None.

## Return value

An ECDH key of the same scalar.

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
    // NIST's first test of ECC CDH on P-256: the scalar dIUT, the peer's point QCAVS
    // and the shared secret ZIUT they give
    auto key = crypto::p256::private_key::from_bytes(encoding::hex::decode(
        "7d7dc5f71eb29ddaf80d6214632eeae03d9058af1fb6d22ed80badb62bc1a534"));
    auto peer = crypto::p256::public_key::from_bytes(encoding::hex::decode(
        "04700c48f77f56584c5cc632ca65640db91b6bacce3a4df6b42ce7cc838833d287"
        "db71e509e3fd9b060ddb20ba5c51dcc5948d46fbf640dfe0441782cab85fa4ac"));
    auto expected_secret = encoding::hex::decode(
        "46fc62106420ff012e54a434fbdd2d25ccc5852060561e68040dd7778997bd7b");

    auto agreement = key->to_ecdh();
    auto shared = agreement.shared_secret(peer);
    println("{}", crypto::constant_time::equal(shared->bytes(), expected_secret));
    println("{}", agreement.public_key() == key->public_key());
}
```

Output:

```text
true
true
```

## See also

- [p256::ecdh_key](../p256-ecdh_key/README.md): what it gives
- [sgcl::crypto::p256::private_key](README.md)
