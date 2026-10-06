[sgcl](../../README.md) › [crypto](../README.md) › [jose](../jose.md)

# sgcl::crypto::jose::jwk_set

```cpp
#include "sgcl/crypto/jose.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto::jose {
    class jwk_set;
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::crypto::jose::jwk_set` is a JWK Set (RFC 7517 §5): the keys of an issuer, what an OpenID provider publishes at
its `jwks_uri` and what a token is verified against, the key chosen by the token's `kid`
([jws::verify](../jose-jws/verify.md), [jwt::verify](../jose-jwt/verify.md),
[jwe::decrypt](../jose-jwe/decrypt.md)). Go's `go-jose` has it as `JSONWebKeySet`.

## Rules

- **A value.** A copy holds the same keys (each a [jwk](../jose-jwk/README.md) handle); [push_back](push_back.md) on
  one is not seen in the other.
- **What is not understood is skipped.** A key of a `kty` or a curve the module does not have is passed over when a
  set is read, as RFC 7517 §5 asks; a key that is malformed fails the whole set.
- **No guessing.** A verification against a set that mixes symmetric keys with public ones is refused, and so is a
  `kid` that two keys of the set have.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](jose-jwk_set.md) | an empty set, or a set of keys |
| [parse](parse.md) | the keys of a JWK Set's JSON (static) |

#### Element access

| Function | Description |
|---|---|
| [operator[]](operator_at.md) | the key at an index |
| [keys](keys.md) | the keys |
| [find](find.md) | the key of a kid |

#### Capacity

| Function | Description |
|---|---|
| [size](size.md) | the number of keys |
| [empty](empty.md) | checks whether the set has none |

#### Modifiers

| Function | Description |
|---|---|
| [push_back](push_back.md) | adds a key |

#### Conversions

| Function | Description |
|---|---|
| [to_json](to_json.md) | the public keys' JSON |
| [to_private_json](to_private_json.md) | every key's JSON with its private members, in a secret_bytes |

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto current = crypto::jose::jwk::generate(crypto::jose::algorithm::es256, {.kid = "2026-10"});
    auto previous = crypto::jose::jwk::generate(crypto::jose::algorithm::es256, {.kid = "2026-04"});
    crypto::jose::jwk_set published{current.public_key(), previous.public_key()};
    auto token = crypto::jose::jws::sign("an old token", previous);
    auto payload = crypto::jose::jws::verify(token, published);
    println("{}", string(payload.value()));
}
```

Output:

```text
an old token
```

## See also

- [jwk](../jose-jwk/README.md)
- [sgcl::crypto::jose](../jose.md)
