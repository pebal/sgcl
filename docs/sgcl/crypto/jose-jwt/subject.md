[sgcl](../../README.md) › [crypto](../README.md) › [jose](../jose.md) › [jwt](README.md)

# sgcl::crypto::jose::jwt::subject

```cpp
string subject() const noexcept;
```

The subject, `sub`: whom the token is about.

## Parameters

None.

## Return value

The subject; empty when the token has none, or one that is not a string.

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
    println("{}", t->subject());
}
```

Output:

```text
alice
```

## See also

- [claims](claims.md)
- [sgcl::crypto::jose::jwt](README.md)
