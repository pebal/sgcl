[sgcl](../../README.md) › [crypto](../README.md) › [jose](../jose.md) › [jwk](README.md)

# sgcl::crypto::jose::jwk::parameters

```cpp
encoding::json parameters() const noexcept;
```

Every public member of the key as one JSON object: those of the key itself (`kty`, `crv`, `x`, `y`, `n`, `e`) and the
others, as read (`kid`, `use`, `alg`, `key_ops`, `x5c`, `x5t#S256` and any of the issuer's own) or made. The private
members are never among them.

## Parameters

None.

## Return value

The members, an object.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto key = crypto::jose::jwk::parse(R"({"kty":"OKP","crv":"Ed25519",
        "x":"11qYAYKxCrfVS_7TyWQHOg7hcvPapiMlrwIaaPcHURo","key_ops":["verify"],"x-team":"web"})");
    auto p = key->parameters();
    println("{} {}", p["key_ops"][0].as_string("?"), p["x-team"].as_string("?"));
}
```

Output:

```text
verify web
```

## See also

- [to_json](to_json.md): the same as text
- [sgcl::crypto::jose::jwk](README.md)
