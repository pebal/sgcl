[sgcl](../../README.md) › [crypto](../README.md) › [jose](../jose.md)

# sgcl::crypto::jose::jwt

```cpp
#include "sgcl/crypto/jose.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto::jose {
    class jwt {
    public:
        struct verify_options;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::crypto::jose::jwt` is a JSON Web Token (RFC 7519): a [JWS](../jose-jws/README.md) in the compact serialization
whose payload is a JSON object of claims — who issued it (`iss`), about whom (`sub`), for whom (`aud`), until when
(`exp`), from when (`nbf`), since when (`iat`), its id (`jti`) and any of the issuer's own. [sign](sign.md) makes one
from the claims in one line; [verify](verify.md) checks its signature under a key or the issuer's key set and then its
claims against [verify_options](../jose-jwt-verify_options.md), and gives the token whose claims are read by name. What
`golang-jwt/jwt` and the JWT half of `go-jose` give a Go program.

## Rules

- **A handle of one word**, read once and never changed: a copy shares it, reading from many threads at once is safe.
- **Verified, then checked.** The signature first, with an algorithm the key allows; then `exp` (refused from
  `exp` + leeway: `errc::expired`), `nbf` and `iat` (refused before `nbf` − leeway or an `iat` past now + leeway:
  `errc::not_yet_valid`), `iss` and `aud` (`errc::verification`). A token without `exp` is refused unless the options
  say not to, and one that names an audience is refused when none is expected (RFC 7519 §4.1.3).
- **A date is a number of seconds** (NumericDate), a fraction allowed; any other value of a date is `errc::malformed`.
- **Only the compact serialization** is a JWT; a nested JWT (a JWE around it) is decrypted by
  [jwe::decrypt](../jose-jwe/decrypt.md) first and verified from its plaintext.

## Member types

| Type | Definition |
|---|---|
| [verify_options](../jose-jwt-verify_options.md) | what the claims are checked against: issuer, audience, the instant, the leeway |

## Member functions

| Function | Description |
|---|---|
| [sign](sign.md) | a token of claims signed by a key (static) |
| [verify](verify.md) | a token verified under a key or a key set, its claims checked (static) |
| [parse_unverified](parse_unverified.md) | a token read without any check (static) |

#### Observers

| Function | Description |
|---|---|
| [claims](claims.md) | every claim |
| [header](header.md) | the protected header |
| [issuer](issuer.md) | `iss` |
| [subject](subject.md) | `sub` |
| [audience](audience.md) | `aud` |
| [id](id.md) | `jti` |
| [expires_at](expires_at.md) | `exp` |
| [not_before](not_before.md) | `nbf` |
| [issued_at](issued_at.md) | `iat` |

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"
#include "sgcl/time.h"

using namespace sgcl;

int main() {
    auto key = crypto::jose::jwk::generate(crypto::jose::algorithm::hs256);
    auto token = crypto::jose::jwt::sign(encoding::json::object({{"sub", "alice"}, {"role", "admin"},
                                                                 {"exp", time::now().unix() + 60}}), key);
    auto t = crypto::jose::jwt::verify(token, key);
    println("{} {}", t->subject(), t->claims()["role"].as_string("?"));
}
```

Output:

```text
alice admin
```

## See also

- [jws](../jose-jws/README.md)
- [verify_options](../jose-jwt-verify_options.md)
- [sgcl::crypto::jose](../jose.md)
