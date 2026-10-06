[sgcl](../README.md) › [crypto](README.md) › [jose](jose.md)

# sgcl::crypto::jose::sign_options

```cpp
#include "sgcl/crypto/jose.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto::jose {
    struct sign_options {
        optional<algorithm> alg;
        encoding::json header;
    };
}
```

**Requires [rooted](../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::crypto::jose::sign_options` is what a signer may set beside the payload and the key, for
[jws::sign](jose-jws/sign.md), [jws::sign_json](jose-jws/sign_json.md) and [jwt::sign](jose-jwt/sign.md): the
algorithm, and more members of the protected header. Every field has a default; a call names the fields it sets:
`jws::sign(payload, key, {.alg = algorithm::ps256})`.

## Member objects

| Member | Description |
|---|---|
| `alg` | the algorithm; `nullopt`, the default: the key's `alg`, else its kind's (oct HS256, RSA RS256, P-256 ES256, P-384 ES384, P-521 ES512, Ed25519 EdDSA) |
| `header` | a JSON object of more members of the protected header (`typ`, `cty`, `kid`, `nonce`, `url`, …), written after `alg` in their order; its `alg` is never taken; a `kid` of its own is taken over the key's; null, the default: none. Anything else than an object or null is `std::invalid_argument` |

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto key = crypto::jose::jwk::parse(R"({"kty":"OKP","crv":"Ed25519","kid":"k1",
        "d":"nWGxne_9WmC6hEr0kuwsxERJxWl7MmkZcDusAxyuf2A","x":"11qYAYKxCrfVS_7TyWQHOg7hcvPapiMlrwIaaPcHURo"})");
    auto token = crypto::jose::jws::sign("hello", *key,
                                         {.header = encoding::json::object({{"typ", "demo"}})});
    println("{}", crypto::jose::jws::parse(token)->header().to_string());
}
```

Output:

```text
{"alg":"EdDSA","kid":"k1","typ":"demo"}
```

## See also

- [jws](jose-jws/README.md), [jwt](jose-jwt/README.md)
- [sgcl::crypto::jose](jose.md)
