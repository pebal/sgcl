[sgcl](../../README.md) › [crypto](../README.md) › [jose](../jose.md) › [jwt](README.md)

# sgcl::crypto::jose::jwt::sign

```cpp
static string sign(const encoding::json& claims, const jwk& key);
static string sign(const encoding::json& claims, const jwk& key, const sign_options& o);
```

A token of the claims, signed by the key: the compact JWS of the claims' compact JSON, as
[jws::sign](../jose-jws/sign.md)
makes it, its header with `typ` `JWT` unless `o.header` names another (`at+jwt` of RFC 9068). The claims are written
as they are given; a date is a number of seconds, `time::now().unix() + 3600`.

## Parameters

| Parameter | Description |
|---|---|
| `claims` | the claims, a JSON object |
| `key` | a private key, or an oct key |
| `o` | the algorithm and the header ([sign_options](../jose-sign_options.md)) |

## Return value

The token.

## Complexity

Linear in the size of the claims, and the signature.

## Exceptions

`std::invalid_argument` for claims that are not a JSON object, and as [jws::sign](../jose-jws/sign.md) has it, for a
key that cannot sign.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto key = crypto::jose::jwk::symmetric("a secret of thirty-two bytes ...");
    println("{}", crypto::jose::jwt::sign(encoding::json::object({{"sub", "alice"}, {"exp", 1800000000}}), key));
}
```

Output:

```text
eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJzdWIiOiJhbGljZSIsImV4cCI6MTgwMDAwMDAwMH0.tElP7DqEjK5k9MnpFjdFqEJNFBIiz01y1fH0popQNw8
```

## See also

- [verify](verify.md)
- [sgcl::crypto::jose::jwt](README.md)
