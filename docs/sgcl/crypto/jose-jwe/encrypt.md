[sgcl](../../README.md) › [crypto](../README.md) › [jose](../jose.md) › [jwe](README.md)

# sgcl::crypto::jose::jwe::encrypt

```cpp
static string encrypt(const slice<const byte>& plaintext, const jwk& key);
static string encrypt(const slice<const byte>& plaintext, const jwk& key, const encrypt_options& o);
```

The compact serialization of the plaintext encrypted to the key (RFC 7516 §7.1): a random content key encrypts it by
`o.enc` (A256GCM by default) under a random IV, and reaches the key by `o.alg`, else the key's `alg`, else its
kind's: RSA-OAEP-256 to an RSA key, ECDH-ES to a P-256, P-384 or X25519 key (an ephemeral key's public half in the
header's `epk`), `dir` with an oct key of other length than 16, 24 or 32 bytes. The header holds `alg`, `enc`, the
key's `kid` when it has one, what the algorithm adds (`epk`, `iv`, `tag`), and the members of `o.header`.

## Parameters

| Parameter | Description |
|---|---|
| `plaintext` | the plaintext, bytes or text |
| `key` | the recipient's key: public or private (its public half encrypts), or an oct key |
| `o` | the algorithm, the content encryption and the header ([encrypt_options](../jose-encrypt_options.md)) |

## Return value

The JWE.

## Complexity

Linear in the length of the plaintext, and the key's management.

## Exceptions

`std::invalid_argument` when the key cannot encrypt with the algorithm: a key of another kind, an algorithm that is
a signature's, an RSA key under 2048 bits, a `dir` key of another length than the content key's, a key whose `alg`,
`use` or `key_ops` forbid it, an AES key wrap key of another size than the algorithm's, a header that is not a JSON object, a value of
no encryption.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto key = crypto::jose::jwk::generate(crypto::jose::algorithm::rsa_oaep_256);
    auto token = crypto::jose::jwe::encrypt("the plan", key.public_key(),
                                            {.enc = crypto::jose::encryption::a128gcm});
    println("{}", crypto::jose::jwe::parse(token)->header().to_string());
    println("{}", string(crypto::jose::jwe::decrypt(token, key).value()));
}
```

Output:

```text
{"alg":"RSA-OAEP-256","enc":"A128GCM"}
the plan
```

## See also

- [decrypt](decrypt.md)
- [sgcl::crypto::jose::jwe](README.md)
