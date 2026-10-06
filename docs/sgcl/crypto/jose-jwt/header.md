[sgcl](../../README.md) › [crypto](../README.md) › [jose](../jose.md) › [jwt](README.md)

# sgcl::crypto::jose::jwt::header

```cpp
encoding::json header() const noexcept;
```

The token's protected header: `alg`, `typ`, `kid` and any other member.

## Parameters

None.

## Return value

The header.

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
    println("{}", t->header().to_string());
}
```

Output:

```text
{"alg":"HS256","typ":"JWT"}
```

## See also

- [jws::header](../jose-jws/header.md)
- [sgcl::crypto::jose::jwt](README.md)
