[sgcl](../../README.md) › [crypto](../README.md) › [jose](../jose.md) › [jwt](README.md)

# sgcl::crypto::jose::jwt::verify

```cpp
static expected<jwt, error> verify(const string& token, const jwk& key) noexcept;
static expected<jwt, error> verify(const string& token, const jwk& key, const verify_options& o) noexcept;
static expected<jwt, error> verify(const string& token, const jwk_set& keys) noexcept;
static expected<jwt, error> verify(const string& token, const jwk_set& keys, const verify_options& o) noexcept;
```

The token, when its signature verifies under the key (or a key of the set: the one of its `kid`), as
[jws::verify](../jose-jws/verify.md) verifies one, and its claims hold against the options
([verify_options](../jose-jwt-verify_options.md)): `exp`, `nbf` and `iat` against the time with the leeway, `iss` and
`aud` against the ones expected.

## Parameters

| Parameter | Description |
|---|---|
| `token` | the token, the compact serialization |
| `key` | the issuer's key: public, private or oct |
| `keys` | the issuer's keys, as its `jwks_uri` publishes them |
| `o` | what the claims are checked against |

## Return value

The token, or an error: what [jws::verify](../jose-jws/verify.md) refuses; `errc::malformed` for a JSON form, claims
that are not a JSON object, a date that is not a number, an `aud` neither a string nor a list of them;
`errc::expired` past `exp` + leeway; `errc::not_yet_valid` before `nbf` − leeway or with an `iat` past now + leeway;
`errc::verification` for a token without `exp` when one is required, an issuer that is not the one expected, an
audience that does not hold the one expected or is there when none is.

## Complexity

Linear in the length of the token, and the verification.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"
#include "sgcl/time.h"

using namespace sgcl;

int main() {
    auto key = crypto::jose::jwk::generate(crypto::jose::algorithm::es256);
    auto token = crypto::jose::jwt::sign(encoding::json::object({{"iss", "https://id.example"},
        {"aud", encoding::json::array({"api", "web"})}, {"exp", time::now().unix() + 60}}), key);
    auto ok = crypto::jose::jwt::verify(token, key.public_key(),
                                        {.issuer = "https://id.example", .audience = "web"});
    println("{}", ok.has_value());
    auto other = crypto::jose::jwt::verify(token, key.public_key(), {.audience = "billing"});
    println("{}", other.error().message());
}
```

Output:

```text
true
sgcl::crypto::jose: JWT: the audience (aud) does not hold the one expected
```

## See also

- [verify_options](../jose-jwt-verify_options.md)
- [parse_unverified](parse_unverified.md): read without checks
- [sgcl::crypto::jose::jwt](README.md)
