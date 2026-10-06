[sgcl](../README.md) › [crypto](README.md) › [jose](jose.md)

# sgcl::crypto::jose::encryption

```cpp
#include "sgcl/crypto/jose.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto::jose {
    enum class encryption : uint8_t {
        a128cbc_hs256 = 1,
        a192cbc_hs384,
        a256cbc_hs512,
        a128gcm,
        a192gcm,
        a256gcm
    };
}
```

The `enc` of a JWE's header (RFC 7518 §5.1): how the content is encrypted under the content key. A key whose `alg` is
one of these names (RFC 7520 §5.6 writes `"alg":"A128GCM"` for a key of `dir`) is a `dir` key of that encryption
alone.

| Value | Description |
|---|---|
| `a128cbc_hs256` | A128CBC-HS256: AES-128-CBC and HMAC-SHA-256, a content key of 32 bytes |
| `a192cbc_hs384` | A192CBC-HS384: AES-192-CBC and HMAC-SHA-384, 48 bytes |
| `a256cbc_hs512` | A256CBC-HS512: AES-256-CBC and HMAC-SHA-512, 64 bytes |
| `a128gcm` | A128GCM: AES-128-GCM, 16 bytes |
| `a192gcm` | A192GCM: AES-192-GCM, 24 bytes |
| `a256gcm` | A256GCM: AES-256-GCM, 32 bytes, the default of [encrypt_options](jose-encrypt_options.md) |

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto key = crypto::jose::jwk::generate(crypto::jose::algorithm::a128gcmkw);
    auto token = crypto::jose::jwe::encrypt("payload", key,
                                            {.enc = crypto::jose::encryption::a128cbc_hs256});
    println("{}", crypto::jose::jwe::parse(token)->header()["enc"].as_string("?"));
}
```

Output:

```text
A128CBC-HS256
```

## See also

- [algorithm](jose-algorithm.md): `alg`
- [encrypt_options](jose-encrypt_options.md)
- [sgcl::crypto::jose](jose.md)
