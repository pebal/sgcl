[sgcl](../../README.md) › [crypto](../README.md) › [jose](../jose.md) › [jwt](README.md)

# sgcl::crypto::jose::jwt::audience

```cpp
vector<string> audience() const;
```

The audience, `aud`: one string or a list of them, given as a list either way.

## Parameters

None.

## Return value

The audience; empty when the token has none.

## Complexity

Linear in the number of values.

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
    println("{}", t->audience().size());
}
```

Output:

```text
1
```

## See also

- [verify_options](../jose-jwt-verify_options.md): `audience`
- [sgcl::crypto::jose::jwt](README.md)
