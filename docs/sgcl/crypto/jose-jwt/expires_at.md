[sgcl](../../README.md) › [crypto](../README.md) › [jose](../jose.md) › [jwt](README.md)

# sgcl::crypto::jose::jwt::expires_at

```cpp
optional<time::datetime> expires_at() const noexcept;
```

The time the token expires, `exp`, in UTC, its seconds (a fraction dropped).

## Parameters

None.

## Return value

The time; `nullopt` when the token has none, or one that is not a number.

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
    println("{}", t->expires_at()->unix());
}
```

Output:

```text
2000000000
```

## See also

- [not_before](not_before.md), [issued_at](issued_at.md)
- [sgcl::crypto::jose::jwt](README.md)
