[sgcl](../../README.md) › [crypto](../README.md) › [jose](../jose.md)

# sgcl::crypto::jose::jws

```cpp
#include "sgcl/crypto/jose.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto::jose {
    class jws;
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::crypto::jose::jws` is a JSON Web Signature (RFC 7515): a payload, a protected header and a signature over both,
in the compact serialization (`header.payload.signature`, what a JWT is) or as JSON, flattened with one signature or
general with several. It is made by [sign](sign.md) and [sign_json](sign_json.md), which return the text, and read by
[parse](parse.md), which reads all three forms and trusts nothing until [verify](verify.md) says a signature holds
under a key; the one-line `jws::verify(text, key)` does both and gives the payload.

## Rules

- **A handle of one word**, read once and never changed: a copy shares it, reading from many threads at once is safe.
- **Nothing read is trusted before [verify](verify.md).** The headers and the
  [unverified_payload](unverified_payload.md)
  are there to choose a key, never to act on.
- **The algorithm is the key's** ([the rules of jose](../jose.md#rules)): a signature verifies only with an algorithm
  the key allows, whatever its header names.
- **A JSON form's headers.** The protected header and the unprotected one are read together and must not share a
  member; `crit` in either, or a `b64` other than true, is `errc::unsupported`.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](jose-jws.md) | a JWS of a text the program spells; a copy |
| [sign](sign.md) | the compact serialization of a payload signed by a key (static) |
| [sign_json](sign_json.md) | the JSON serialization, flattened or general (static) |
| [parse](parse.md) | a JWS read, nothing verified (static) |
| [verify](verify.md) | the payload when a signature verifies under a key or a key set |

#### Observers

| Function | Description |
|---|---|
| [signature_count](signature_count.md) | the number of signatures |
| [header](header.md) | the header of a signature |
| [alg](alg.md) | the algorithm a signature names |
| [kid](kid.md) | the key a signature names |
| [unverified_payload](unverified_payload.md) | the payload, before any verification |

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // RFC 8037 A.1's key, A.4's JWS
    auto key = crypto::jose::jwk::parse(R"({"kty":"OKP","crv":"Ed25519",
        "d":"nWGxne_9WmC6hEr0kuwsxERJxWl7MmkZcDusAxyuf2A","x":"11qYAYKxCrfVS_7TyWQHOg7hcvPapiMlrwIaaPcHURo"})");
    auto token = crypto::jose::jws::sign("Example of Ed25519 signing", *key);
    println("{}", token);
    auto payload = crypto::jose::jws::verify(token, key->public_key());
    println("{}", string(payload.value()));
}
```

Output:

```text
eyJhbGciOiJFZERTQSJ9.RXhhbXBsZSBvZiBFZDI1NTE5IHNpZ25pbmc.hgyY0il_MGCjP0JzlnLWG1PPOt7-09PGcvMg3AIbQR6dWbhijcNR4ki4iylGjg5BhVsPt9g7sVvpAr_MuM0KAg
Example of Ed25519 signing
```

## See also

- [jwt](../jose-jwt/README.md): a JWS of claims
- [sign_options](../jose-sign_options.md)
- [sgcl::crypto::jose](../jose.md)
