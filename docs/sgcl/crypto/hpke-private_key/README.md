[sgcl](../../README.md) › [crypto](../README.md) › [hpke](../hpke.md)

# sgcl::crypto::hpke::private_key

```cpp
#include "sgcl/crypto/hpke.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto::hpke {
    class private_key;
}
```

`sgcl::crypto::hpke::private_key` is a KEM private key of HPKE: the recipient's, which opens what is sealed to its
[public key](../hpke-public_key/README.md), or a sender's of the auth modes. Made new, derived from a seed (RFC 9180's
DeriveKeyPair, the same key every time), or read from its bytes. Go's `hpke.PrivateKey`.

## Rules

- **The scalar lies in the object** (no allocation): the key is move-only, [clone](clone.md) is the copy by name, and a
  move and the destructor zero it. It belongs on the stack or in a `unique_ptr`, as the module's keys do.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](hpke-private_key.md) | a key moved in |
| [generate](generate.md) | a new key of a KEM (static) |
| [derive](derive.md) | the key of a seed (static) |
| [from_bytes](from_bytes.md) | a key of its bytes (static) |
| [clone](clone.md) | a copy of the key |
| [kem](kem.md) | the key's KEM |
| [bytes](bytes.md) | the key's bytes, in a secret_bytes |
| [public_key](public_key.md) | the public half |

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto key = crypto::hpke::private_key::generate(crypto::hpke::kem::dhkem_p256);
    auto sealed = crypto::hpke::seal(key.public_key(), "to the holder of the key");
    println("{}", string(crypto::hpke::open(key, sealed.value()).value()));
}
```

Output:

```text
to the holder of the key
```

## See also

- [public_key](../hpke-public_key/README.md)
- [sgcl::crypto::hpke](../hpke.md)
