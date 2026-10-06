[sgcl](../../README.md) › [crypto](../README.md) › [hpke](../hpke.md) › [sender](README.md)

# sgcl::crypto::hpke::sender::setup

```cpp
static expected<sender, error> setup(const public_key& to, const suite& s);
static expected<sender, error> setup(const public_key& to, const suite& s, const slice<const byte>& info);
static expected<sender, error> setup(const public_key& to, const suite& s, const slice<const byte>& info,
                                     const options& o);
static expected<sender, error> setup(const public_key& to, const suite& s, const slice<const byte>& info,
                                     const private_key& sender_key);
static expected<sender, error> setup(const public_key& to, const suite& s, const slice<const byte>& info,
                                     const options& o, const private_key& sender_key);
```

A context sealing to `to` (RFC 9180 §5.1): a fresh ephemeral key of the KEM, the shared secret of it and `to` (and of
the sender's key, in the auth modes), and the key schedule of the suite over it, `info` and the PSK. The mode follows
from what is given: SetupBaseS without options, SetupPSKS with a PSK in `o`, SetupAuthS with `sender_key`,
SetupAuthPSKS with both.

## Parameters

| Parameter | Description |
|---|---|
| `to` | the recipient's public key: its KEM is the suite's |
| `s` | the suite |
| `info` | what the application binds the context to (a protocol's name, a version), the same on both sides; none by default |
| `o` | the PSK and its id ([options](../hpke-options.md)) |
| `sender_key` | the sender's private key, of the recipient's KEM: the recipient knows the messages come from it (the auth modes) |

## Return value

The context, or `errc::invalid_key` for an X25519 key of small order, whose shared secret is zero.

## Complexity

Constant: a key generated and one or two Diffie-Hellman operations.

## Exceptions

`std::invalid_argument` for a sender's key of another KEM than the recipient's, a PSK without its id
or the reverse, and a suite of ids of no value.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto key = crypto::hpke::private_key::generate(crypto::hpke::kem::dhkem_x25519);
    auto mine = crypto::hpke::private_key::generate(crypto::hpke::kem::dhkem_x25519);
    auto s = crypto::hpke::sender::setup(key.public_key(), {}, "app/v1", mine);
    auto sealed = s->seal("from the one key");
    auto r = crypto::hpke::recipient::setup(s->enc(), key, {}, "app/v1", mine.public_key());
    println("{}", string(r->open(sealed).value()));
}
```

Output:

```text
from the one key
```

## See also

- [recipient::setup](../hpke-recipient/setup.md)
- [sgcl::crypto::hpke::sender](README.md)
