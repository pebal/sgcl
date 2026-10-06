[sgcl](../../README.md) › [crypto](../README.md) › [hpke](../hpke.md)

# sgcl::crypto::hpke::recipient

```cpp
#include "sgcl/crypto/hpke.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto::hpke {
    class recipient;
}
```

`sgcl::crypto::hpke::recipient` is a receiving context of HPKE (RFC 9180 §5.2): made of the sender's
[enc](../hpke-sender/enc.md) and the recipient's [private key](../hpke-private_key/README.md), it opens the sender's
messages in the order they were sealed and exports the secrets the sender exports. Go's `hpke.Recipient`.

## Rules

- **Move-only, its secrets in the object**, zeroed by the destructor and in a context moved from, whose calls are then
  `std::logic_error`.
- **One thread at a time**: [open](open.md) counts the messages; one that does not open leaves the count where it was.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](hpke-recipient.md) | a context moved in |
| [setup](setup.md) | a context of enc and a private key, in a mode (static) |
| [suite](suite.md) | the context's suite |
| [open](open.md) | the next message opened |
| [export_secret](export_secret.md) | a secret exported from the context |

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto key = crypto::hpke::private_key::generate(crypto::hpke::kem::dhkem_p256);
    auto s = crypto::hpke::sender::setup(key.public_key(), {});
    auto sealed = s->seal("over P-256");
    auto r = crypto::hpke::recipient::setup(s->enc(), key, {});
    println("{}", string(r->open(sealed).value()));
}
```

Output:

```text
over P-256
```

## See also

- [sender](../hpke-sender/README.md)
- [open](../hpke-open.md): one message in one line
- [sgcl::crypto::hpke](../hpke.md)
