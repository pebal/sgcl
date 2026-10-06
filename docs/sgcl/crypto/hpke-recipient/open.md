[sgcl](../../README.md) › [crypto](../README.md) › [hpke](../hpke.md) › [recipient](README.md)

# sgcl::crypto::hpke::recipient::open

```cpp
expected<vector<byte>, error> open(const slice<const byte>& ciphertext);
expected<vector<byte>, error> open(const slice<const byte>& ciphertext, const slice<const byte>& aad);
```

The next message opened (RFC 9180 §5.2): the AEAD's open under the base nonce XOR the count of messages opened so far,
`aad` checked with it. A message that does not open leaves the count where it was, so the next one still opens.
`[[nodiscard]]`.

## Parameters

| Parameter | Description |
|---|---|
| `ciphertext` | what the sender's [seal](../hpke-sender/seal.md) gave |
| `aad` | the sender's aad; none by default |

## Return value

The message, or `errc::authentication` for a message that does not open: another one, out of order, changed, under
another aad.

## Complexity

Linear in the length of the message.

## Exceptions

`std::logic_error` for an `export_only` suite and a context moved from.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto key = crypto::hpke::private_key::generate(crypto::hpke::kem::dhkem_x25519);
    auto s = crypto::hpke::sender::setup(key.public_key(), {}, "app");
    auto r = crypto::hpke::recipient::setup(s->enc(), key, {}, "app");
    auto first = s->seal("one");
    auto second = s->seal("two");
    println("{}", r->open(second).has_value());
    println("{} {}", string(r->open(first).value()), string(r->open(second).value()));
}
```

Output:

```text
false
one two
```

## See also

- [sender::seal](../hpke-sender/seal.md)
- [sgcl::crypto::hpke::recipient](README.md)
