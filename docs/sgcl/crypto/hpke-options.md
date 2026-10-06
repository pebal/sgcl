[sgcl](../README.md) › [crypto](README.md) › [hpke](hpke.md)

# sgcl::crypto::hpke::options

```cpp
#include "sgcl/crypto/hpke.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto::hpke {
    struct options {
        slice<const byte> psk;
        slice<const byte> psk_id;
    };
}
```

**Requires [rooted](../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::crypto::hpke::options` are the pre-shared key of the PSK mode (RFC 9180 §5.1), given to
[sender::setup](hpke-sender/setup.md) and [recipient::setup](hpke-recipient/setup.md): only a sender that knows the
PSK could have sealed the messages. The sender's key of the auth mode is a parameter of its own of the two setups;
with both, the mode is auth-PSK.

## Rules

- The slices are read during the call; nothing is kept.
- A PSK and its id are given together or not at all; the PSK should have 32 bytes of entropy (RFC 9180 §9.5).

## Member objects

| Member | Description |
|---|---|
| `psk` | the pre-shared key; empty, the default: none |
| `psk_id` | its id, which both sides know it by; empty with `psk` |

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto key = crypto::hpke::private_key::generate(crypto::hpke::kem::dhkem_x25519);
    crypto::hpke::options o{.psk = "a pre-shared key of 32 bytes ...", .psk_id = "2026"};
    auto s = crypto::hpke::sender::setup(key.public_key(), {}, "app", o);
    auto sealed = s->seal("from one who knows the key");
    auto r = crypto::hpke::recipient::setup(s->enc(), key, {}, "app", o);
    println("{}", string(r->open(sealed).value()));
}
```

Output:

```text
from one who knows the key
```

## See also

- [sender::setup](hpke-sender/setup.md), [recipient::setup](hpke-recipient/setup.md)
- [sgcl::crypto::hpke](hpke.md)
