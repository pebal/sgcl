[sgcl](../../README.md) › [crypto](../README.md) › [hpke](../hpke.md) › [sender](README.md)

# sgcl::crypto::hpke::sender::seal

```cpp
vector<byte> seal(const slice<const byte>& plaintext);
vector<byte> seal(const slice<const byte>& plaintext, const slice<const byte>& aad);
```

The next message sealed (RFC 9180 §5.2): the AEAD's seal under the base nonce XOR the count of messages sealed so far,
with `aad` authenticated and not sent. The recipient opens it as its message of the same number.

## Parameters

| Parameter | Description |
|---|---|
| `plaintext` | the message, bytes or text |
| `aad` | data authenticated with it, not sealed: a header, an id; none by default |

## Return value

The ciphertext and its tag of 16 bytes.

## Complexity

Linear in the length of the message.

## Exceptions

`std::logic_error` for an `export_only` suite and a context moved from; `std::length_error` when 2^64 - 1 messages
were sealed (MessageLimitReachedError); `std::length_error` for a message past the AEAD's limit.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto key = crypto::hpke::private_key::generate(crypto::hpke::kem::dhkem_x25519);
    auto s = crypto::hpke::sender::setup(key.public_key(), {}, "app");
    auto r = crypto::hpke::recipient::setup(s->enc(), key, {}, "app");
    auto c = s->seal("hello", "header");
    println("{} {}", c.size(), string(r->open(c, "header").value()));
}
```

Output:

```text
21 hello
```

## See also

- [recipient::open](../hpke-recipient/open.md)
- [sgcl::crypto::hpke::sender](README.md)
