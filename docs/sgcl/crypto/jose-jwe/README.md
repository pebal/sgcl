[sgcl](../../README.md) › [crypto](../README.md) › [jose](../jose.md)

# sgcl::crypto::jose::jwe

```cpp
#include "sgcl/crypto/jose.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto::jose {
    class jwe;
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::crypto::jose::jwe` is a JSON Web Encryption (RFC 7516) in the compact serialization: a protected header, the
content key encrypted to the recipient, an IV, the ciphertext and the tag, five parts in base64url joined by dots. A
random content key encrypts the content (AES-GCM, or AES-CBC with HMAC); the key reaches the recipient by the
algorithm the header names: encrypted to an RSA key (RSA-OAEP), agreed with an ephemeral key (ECDH-ES on P-256,
P-384, P-521 or X25519, alone or wrapping the content key), wrapped under a shared key (AES key wrap, AES-GCM key
wrap), or the shared key itself (`dir`). It is made by
[encrypt](encrypt.md), which returns the text, and read by [parse](parse.md); [decrypt](decrypt.md) opens it, and the
one-line `jwe::decrypt(text, key)` does both.

## Rules

- **A handle of one word**, read once and never changed: a copy shares it, reading from many threads at once is safe.
- **The header is authenticated** (it is the content's additional data): a part changed anywhere fails the tag,
  `errc::authentication`.
- **A key that does not unwrap fails as a wrong tag fails.** RSA-OAEP's failure goes on with a random content key
  (RFC 7516 §11.5), so that a forger cannot tell the two apart; the tag of CBC-HMAC is checked in constant time
  before a byte is decrypted, the padding after.
- **The plaintext is the program's data**, a `vector<byte>`; the content key lives in plain memory and is zeroed.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](jose-jwe.md) | a JWE of a text the program spells; a copy |
| [encrypt](encrypt.md) | the compact serialization of a plaintext encrypted to a key (static) |
| [parse](parse.md) | a JWE read (static) |
| [decrypt](decrypt.md) | the plaintext, under a key or a key set |

#### Observers

| Function | Description |
|---|---|
| [header](header.md) | the protected header |
| [alg](alg.md) | the key management the header names |
| [enc](enc.md) | the content encryption the header names |
| [kid](kid.md) | the key the header names |

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto recipient = crypto::jose::jwk(crypto::x25519::private_key::generate(), {.kid = "inbox"});
    auto token = crypto::jose::jwe::encrypt("meet at noon", recipient.public_key());
    println("{}", crypto::jose::jwe::parse(token)->alg() == crypto::jose::algorithm::ecdh_es);
    println("{}", string(crypto::jose::jwe::decrypt(token, recipient).value()));
}
```

Output:

```text
true
meet at noon
```

## See also

- [encrypt_options](../jose-encrypt_options.md)
- [aes_gcm](../aes_gcm/README.md), [x25519](../x25519.md): what encrypts and agrees keys under it
- [sgcl::crypto::jose](../jose.md)
