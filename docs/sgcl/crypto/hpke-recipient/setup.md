[sgcl](../../README.md) › [crypto](../README.md) › [hpke](../hpke.md) › [recipient](README.md)

# sgcl::crypto::hpke::recipient::setup

```cpp
static expected<recipient, error> setup(const slice<const byte>& enc, const private_key& key, const suite& s) noexcept;
static expected<recipient, error> setup(const slice<const byte>& enc, const private_key& key, const suite& s,
                                        const slice<const byte>& info) noexcept;
static expected<recipient, error> setup(const slice<const byte>& enc, const private_key& key, const suite& s,
                                        const slice<const byte>& info, const options& o) noexcept;
static expected<recipient, error> setup(const slice<const byte>& enc, const private_key& key, const suite& s,
                                        const slice<const byte>& info, const public_key& sender_public) noexcept;
static expected<recipient, error> setup(const slice<const byte>& enc, const private_key& key, const suite& s,
                                        const slice<const byte>& info, const options& o,
                                        const public_key& sender_public) noexcept;
```

A context opening what the sender of `enc` seals (RFC 9180 §5.1): the shared secret of `enc` and `key` (and of the
sender's public key, in the auth modes), and the key schedule of the suite over it, `info` and the PSK. The mode
follows from what is given as on the [sender's side](../hpke-sender/setup.md): it must be the sender's, with the
same PSK and the public key of the sender's key, or nothing opens.

## Parameters

| Parameter | Description |
|---|---|
| `enc` | the sender's [enc](../hpke-sender/enc.md) |
| `key` | the recipient's private key: its KEM is the suite's |
| `s` | the suite, the sender's |
| `info` | the sender's info; none by default |
| `o` | the PSK and its id ([options](../hpke-options.md)) |
| `sender_public` | the public key of the sender's key (the auth modes) |

## Return value

The context, or an error: `errc::invalid_key` for an `enc` that is not a public key of the KEM or gives the zero
secret, a sender's key of another KEM than the recipient's; `errc::malformed` for a PSK without its id or the
reverse; `errc::unsupported` for a suite of ids of no value.

## Complexity

Constant: one or two Diffie-Hellman operations.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto key = crypto::hpke::private_key::generate(crypto::hpke::kem::dhkem_x25519);
    auto bad = crypto::hpke::recipient::setup("too short", key, {});
    println("{}", bad.error().message());
}
```

Output:

```text
sgcl::crypto::hpke: enc is not a public key of the KEM
```

## See also

- [sender::setup](../hpke-sender/setup.md)
- [sgcl::crypto::hpke::recipient](README.md)
