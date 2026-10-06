[sgcl](../README.md) › [crypto](README.md) › [jose](jose.md) › [jwt](jose-jwt/README.md)

# sgcl::crypto::jose::jwt::verify_options

```cpp
#include "sgcl/crypto/jose.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto::jose {
    class jwt {
    public:
        struct verify_options {
            string issuer;
            string audience;
            duration leeway = minute;
            optional<time::datetime> at;
            bool require_expiration = true;
        };
    };
}
```

**Requires [rooted](../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::crypto::jose::jwt::verify_options` is what [jwt::verify](jose-jwt/verify.md) checks a token's claims against once
its signature verifies (RFC 7519 §4.1): who issued it, whom it is for, and when it is valid, with the skew two clocks
may have. Every field has a default: `jwt::verify(token, key, {.audience = "api"})`.

## Member objects

| Member | Description |
|---|---|
| `issuer` | the `iss` the token must have; empty, the default: not checked |
| `audience` | a value the token's `aud` (a string or a list) must hold; empty, the default: a token that names an audience is refused, as RFC 7519 §4.1.3 has it |
| `leeway` | the skew allowed: a token is valid until `exp` + leeway and from `nbf` − leeway, and an `iat` up to leeway in the future; a minute by default |
| `at` | the instant checked against; `nullopt`, the default: `time::now()` |
| `require_expiration` | whether a token without `exp` is refused; true by default |

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"
#include "sgcl/time.h"

using namespace sgcl;

int main() {
    auto key = crypto::jose::jwk::generate(crypto::jose::algorithm::hs256);
    auto token = crypto::jose::jwt::sign(encoding::json::object({{"exp", 1800000000}}), key);
    for (int64_t t : {1799999999, 1800000059, 1800000060}) {
        auto r = crypto::jose::jwt::verify(token, key, {.at = time::datetime::from_unix(t)});
        println("{}: {}", t, r ? "valid" : r.error().message());
    }
}
```

Output:

```text
1799999999: valid
1800000059: valid
1800000060: sgcl::crypto::jose: JWT: the token has expired (exp)
```

## See also

- [jwt::verify](jose-jwt/verify.md)
- [sgcl::crypto::jose::jwt](jose-jwt/README.md)
