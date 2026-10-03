[sgcl](../../README.md) › [crypto](../README.md) › [ed25519](../ed25519.md) › [private_key](../ed25519-private_key.md)

# sgcl::crypto::ed25519::private_key::sign

```cpp
array<byte, 64> sign(const slice<const byte>& message) const;
```

Returns the signature of `message`, RFC 8032 §5.1.6: r = SHA-512(prefix ‖ M) mod L, R = r·B, k = SHA-512(R ‖ A ‖ M)
mod L, S = r + k·s mod L, and the signature is R ‖ S. It is deterministic: the same key and message give the same
signature, and no random number is drawn. The message is bytes or text: `sign("text")` signs the text's bytes, as
`sign(bytes)` does. Pure Ed25519: the message is signed as it is, not a digest of it. Go's `ed25519.Sign`.

The multiplication r·B goes through a table read whole with masks, and S is computed by arithmetic with no branch on
the values: the nonce r, the scalar and the prefix are secret, and every value computed from them until S is zeroed
before the call returns. The signature is public, and comes as an `array<byte, 64>`.

## Parameters

| Parameter | Description |
|---|---|
| `message` | the message, bytes or text |

## Return value

The signature, 64 bytes.

## Complexity

Linear in the size of `message`, which is hashed twice, and one fixed-base multiplication.

## Exceptions

`logic_error` when the key was moved from.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // RFC 8032's TEST 1: the empty message
    auto key = crypto::ed25519::private_key::from_seed(
        encoding::hex::decode("9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60"));
    auto signature = key->sign("");
    println("{}", encoding::hex::encode(signature.as_slice().subslice(0, 32)));
    println("{}", encoding::hex::encode(signature.as_slice().subslice(32)));
    println("{}", key->sign("") == signature);
}
```

Output:

```text
e5564300c360ac729086e2cc806e828a84877f1eb8e5d974d873e06522490155
5fb8821590a33bacc61e39701cf9b46bd25bf5f0595bbe24655141438e7a100b
true
```

## See also

- [ed25519::public_key::verify](../ed25519-public_key/verify.md): the check of a signature
- [public_key](public_key.md): the key that verifies
- [sgcl::crypto::ed25519::private_key](../ed25519-private_key.md)
