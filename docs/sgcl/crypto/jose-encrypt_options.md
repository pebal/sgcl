[sgcl](../README.md) › [crypto](README.md) › [jose](jose.md)

# sgcl::crypto::jose::encrypt_options

```cpp
#include "sgcl/crypto/jose.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto::jose {
    struct encrypt_options {
        optional<algorithm> alg;
        encryption enc = encryption::a256gcm;
        encoding::json header;
    };
}
```

**Requires [rooted](../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::crypto::jose::encrypt_options` is what [jwe::encrypt](jose-jwe/encrypt.md) may be told beside the plaintext
and the key: how the content key reaches the recipient, how the content is encrypted, and more members of the
protected header. Every field has a default: `jwe::encrypt(data, key, {.enc = encryption::a128cbc_hs256})`.

## Member objects

| Member | Description |
|---|---|
| `alg` | the key management; `nullopt`, the default: the key's `alg`, else its kind's (RSA RSA-OAEP-256, P-256, P-384, P-521 and X25519 ECDH-ES, an oct key of 16, 24 or 32 bytes the AES key wrap of its size, any other oct key `dir`) |
| `enc` | the content encryption; A256GCM by default |
| `header` | a JSON object of more members of the protected header (`typ`, `cty`, …); its `alg` and `enc` are never taken; null, the default: none |

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto key = crypto::jose::jwk::symmetric(crypto::random::secret(32));
    auto token = crypto::jose::jwe::encrypt("hello", key,
        {.alg = crypto::jose::algorithm::dir, .enc = crypto::jose::encryption::a128cbc_hs256,
         .header = encoding::json::object({{"cty", "text/plain"}})});
    println("{}", crypto::jose::jwe::parse(token)->header().to_string());
}
```

Output:

```text
{"alg":"dir","enc":"A128CBC-HS256","cty":"text/plain"}
```

## See also

- [jwe](jose-jwe/README.md)
- [sgcl::crypto::jose](jose.md)
