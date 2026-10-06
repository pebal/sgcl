[sgcl](../README.md) › [crypto](README.md) › [jose](jose.md)

# sgcl::crypto::jose::algorithm

```cpp
#include "sgcl/crypto/jose.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto::jose {
    enum class algorithm : uint8_t {
        hs256 = 1,
        hs384,
        hs512,
        rs256,
        rs384,
        rs512,
        ps256,
        ps384,
        ps512,
        es256,
        es384,
        es512,
        eddsa,
        rsa_oaep,
        rsa_oaep_256,
        a128kw,
        a192kw,
        a256kw,
        a128gcmkw,
        a192gcmkw,
        a256gcmkw,
        dir,
        ecdh_es,
        ecdh_es_a128kw,
        ecdh_es_a192kw,
        ecdh_es_a256kw
    };
}
```

The `alg` of a header and of a key (RFC 7518 §3.1 and §4.1, RFC 8037 §3.1): the algorithm of a JWS's signature or of
a JWE's key management, one list as the registry has it. A header names it by its text (`"ES256"`), the program by
this value. A signature algorithm given where a key management one is asked, or the other way, is
`std::invalid_argument`.

| Value | Description |
|---|---|
| `hs256`, `hs384`, `hs512` | HS256, HS384, HS512: HMAC with SHA-2, an oct key at least as long as the digest |
| `rs256`, `rs384`, `rs512` | RS256, RS384, RS512: RSASSA-PKCS1-v1_5, an RSA key of 2048 bits or more |
| `ps256`, `ps384`, `ps512` | PS256, PS384, PS512: RSASSA-PSS, MGF1 over the same digest, the salt as long as the digest |
| `es256`, `es384`, `es512` | ES256, ES384, ES512: ECDSA on P-256, P-384 and P-521, the signature r ‖ s of fixed size |
| `eddsa` | EdDSA: Ed25519 (RFC 8037) |
| `rsa_oaep`, `rsa_oaep_256` | RSA-OAEP (SHA-1), RSA-OAEP-256 (SHA-256): the content key encrypted to an RSA key |
| `a128kw`, `a192kw`, `a256kw` | A128KW, A192KW, A256KW: the content key wrapped under an oct key of 16, 24 or 32 bytes (AES key wrap, RFC 3394) |
| `a128gcmkw`, `a192gcmkw`, `a256gcmkw` | A128GCMKW, A192GCMKW, A256GCMKW: the content key sealed by AES-GCM, its iv and tag in the header |
| `dir` | dir: the key is the content key |
| `ecdh_es` | ECDH-ES: the content key agreed with an ephemeral key (P-256, P-384, X25519) through the Concat KDF |
| `ecdh_es_a128kw`, `ecdh_es_a192kw`, `ecdh_es_a256kw` | ECDH-ES+A128KW, +A192KW, +A256KW: a key agreed with an ephemeral key (the Concat KDF of the alg) that wraps the content key |

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto key = crypto::jose::jwk::generate(crypto::jose::algorithm::ps256);
    auto token = crypto::jose::jws::sign("payload", key);
    auto read = crypto::jose::jws::parse(token);
    println("{}", read->alg() == crypto::jose::algorithm::ps256);
    println("{}", read->header()["alg"].as_string("?"));
}
```

Output:

```text
true
PS256
```

## See also

- [encryption](jose-encryption.md): `enc`
- [jwk::alg](jose-jwk/alg.md): the algorithm a key is for
- [sgcl::crypto::jose](jose.md)
