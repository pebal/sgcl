[sgcl](../../README.md) › [crypto](../README.md) › [jose](../jose.md) › [jwt](README.md)

# sgcl::crypto::jose::jwt::claims

```cpp
encoding::json claims() const noexcept;
```

Every claim of the token, a JSON object as it was signed: the registered ones and the issuer's own.

## Parameters

None.

## Return value

The claims.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto key = crypto::jose::jwk::generate(crypto::jose::algorithm::hs256);
    auto claims = encoding::json::object({{"iss", "https://id.example"}, {"sub", "alice"}, {"aud", "api"},
        {"jti", "t-1"}, {"exp", 2000000000}, {"nbf", 1700000000}, {"iat", 1700000000}, {"scope", "read"}});
    auto t = crypto::jose::jwt::verify(crypto::jose::jwt::sign(claims, key), key, {.audience = "api"});
    println("{}", t->claims()["scope"].as_string("?"));
}
```

Output:

```text
read
```

## See also

- [issuer](issuer.md), [subject](subject.md): the registered ones by name
- [sgcl::crypto::jose::jwt](README.md)
