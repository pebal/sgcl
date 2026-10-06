[sgcl](../README.md) › [crypto](README.md) › [hpke](hpke.md)

# sgcl::crypto::hpke::seal

```cpp
expected<vector<byte>, error> seal(const public_key& to, const slice<const byte>& plaintext, const suite& s = {},
                                   const slice<const byte>& info = {}, const slice<const byte>& aad = {});
```

One message sealed to a public key (RFC 9180 §6.1, Go's `hpke.Seal`): a [sender](hpke-sender/README.md) made to `to`,
its [enc](hpke-sender/enc.md), and the message sealed after it — what [open](hpke-open.md) takes.

## Parameters

| Parameter | Description |
|---|---|
| `to` | the recipient's public key |
| `plaintext` | the message, bytes or text |
| `s` | the suite; the default: X25519, HKDF-SHA256, AES-128-GCM |
| `info` | what the application binds it to; none by default |
| `aad` | data authenticated with it, not sealed; none by default |

## Return value

enc followed by the ciphertext and its tag, or `errc::invalid_key` for an X25519 key of small order.

## Complexity

Linear in the length of the message, and a key generated and a Diffie-Hellman operation.

## Exceptions

`std::invalid_argument` as [sender::setup](hpke-sender/setup.md) has it; `std::logic_error` for an `export_only` suite.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto key = crypto::hpke::private_key::generate(crypto::hpke::kem::dhkem_p384);
    crypto::hpke::suite s{crypto::hpke::kdf::hkdf_sha384, crypto::hpke::aead::aes256_gcm};
    auto sealed = crypto::hpke::seal(key.public_key(), "top secret", s);
    println("{}", sealed->size());
}
```

Output:

```text
123
```

## See also

- [open](hpke-open.md)
- [sgcl::crypto::hpke](hpke.md)
