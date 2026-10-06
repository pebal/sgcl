[sgcl](../../README.md) › [crypto](../README.md) › [hpke](../hpke.md) › [sender](README.md)

# sgcl::crypto::hpke::sender::enc

```cpp
slice<const byte> enc() const noexcept;
```

The encapsulated key (SerializePublicKey of the ephemeral key): what the recipient's
[setup](../hpke-recipient/setup.md) takes first, sent before the messages or in front of them. Not a secret. A view of
the bytes the context holds in itself, valid while the context lives: a program that keeps it copies it.

## Parameters

None.

## Return value

The bytes: 32 of X25519, 65 of P-256, 97 of P-384, 133 of P-521.

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
    auto key = crypto::hpke::private_key::generate(crypto::hpke::kem::dhkem_x25519);
    auto s = crypto::hpke::sender::setup(key.public_key(), {}, "app");
    auto r = crypto::hpke::recipient::setup(s->enc(), key, {}, "app");
    println("{}", s->enc().size());
}
```

Output:

```text
32
```

## See also

- [recipient::setup](../hpke-recipient/setup.md)
- [sgcl::crypto::hpke::sender](README.md)
