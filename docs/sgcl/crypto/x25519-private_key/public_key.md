[sgcl](../../README.md) › [crypto](../README.md) › [x25519](../x25519.md) › [private_key](../x25519-private_key.md)

# sgcl::crypto::x25519::private_key::public_key

```cpp
x25519::public_key public_key() const;
```

Returns the public key, what the other side of the exchange is sent. It was computed when the key was made, by
Ed25519's constant-time fixed-base multiplication mapped to the Montgomery curve; this is a copy of 32 bytes.

## Parameters

None.

## Return value

The [public key](../x25519-public_key.md).

## Complexity

Constant.

## Exceptions

`logic_error` when the key was moved from.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // RFC 7748 §6.1's private key of Alice
    auto alice = crypto::x25519::private_key::from_bytes(
        encoding::hex::decode("77076d0a7318a57d3c16c17251b26645df4c2f87ebc0992ab177fba51db92c2a"));
    crypto::x25519::public_key sent = alice->public_key();
    println("{}", encoding::hex::encode(sent.bytes()));
}
```

Output:

```text
8520f0098930a754748b7ddcb43ef75a0dbf3a0d26381af4eba4a98eaa9b4e6a
```

## See also

- [shared_secret](shared_secret.md): what the peer computes with it
- [x25519::public_key](../x25519-public_key.md)
- [sgcl::crypto::x25519::private_key](../x25519-private_key.md)
