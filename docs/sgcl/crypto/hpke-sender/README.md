[sgcl](../../README.md) › [crypto](../README.md) › [hpke](../hpke.md)

# sgcl::crypto::hpke::sender

```cpp
#include "sgcl/crypto/hpke.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto::hpke {
    class sender;
}
```

`sgcl::crypto::hpke::sender` is a sending context of HPKE (RFC 9180 §5.2): made to a recipient's
[public key](../hpke-public_key/README.md) with a fresh ephemeral key, it gives [enc](enc.md), the encapsulated key the
recipient needs first, and then seals messages in order, each under the next nonce, and exports secrets the recipient
can export too. Go's `hpke.Sender`.

## Rules

- **Move-only, its secrets in the object.** The AEAD's key, the base nonce and the exporter secret are zeroed by the
  destructor and in a context moved from, whose calls are then `std::logic_error`.
- **One thread at a time**: [seal](seal.md) counts the messages.
- **The order is the recipient's**: the recipient opens the messages in the order they were sealed.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](hpke-sender.md) | a context moved in |
| [setup](setup.md) | a context made to a public key, in a mode (static) |
| [enc](enc.md) | the encapsulated key, for the recipient |
| [suite](suite.md) | the context's suite |
| [seal](seal.md) | the next message sealed |
| [export_secret](export_secret.md) | a secret exported from the context |

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto key = crypto::hpke::private_key::generate(crypto::hpke::kem::dhkem_x25519);
    auto s = crypto::hpke::sender::setup(key.public_key(), {}, "chat");
    auto first = s->seal("hello");
    auto second = s->seal("again");
    auto r = crypto::hpke::recipient::setup(s->enc(), key, {}, "chat");
    println("{}", string(r->open(first).value()));
    println("{}", string(r->open(second).value()));
}
```

Output:

```text
hello
again
```

## See also

- [recipient](../hpke-recipient/README.md)
- [seal](../hpke-seal.md): one message in one line
- [sgcl::crypto::hpke](../hpke.md)
