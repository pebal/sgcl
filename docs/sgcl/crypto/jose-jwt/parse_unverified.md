[sgcl](../../README.md) › [crypto](../README.md) › [jose](../jose.md) › [jwt](README.md)

# sgcl::crypto::jose::jwt::parse_unverified

```cpp
static expected<jwt, error> parse_unverified(const string& token) noexcept;
```

A token read without verifying its signature or checking a claim: to look at its issuer or its header before the key
is chosen (an issuer of many tenants, a token to log). Nothing in it is to be trusted; [verify](verify.md) is what a
program acts on.

## Parameters

| Parameter | Description |
|---|---|
| `token` | the token |

## Return value

The token, or an error: what [jws::parse](../jose-jws/parse.md) refuses, a JSON form, claims that are not a JSON
object (`errc::malformed`).

## Complexity

Linear in the length of the token.

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
    auto token = crypto::jose::jwt::sign(encoding::json::object({{"iss", "https://tenant-7.example"}}), key);
    println("{}", crypto::jose::jwt::parse_unverified(token)->issuer());
}
```

Output:

```text
https://tenant-7.example
```

## See also

- [verify](verify.md)
- [sgcl::crypto::jose::jwt](README.md)
