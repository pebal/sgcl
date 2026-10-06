[sgcl](../../README.md) › [crypto](../README.md) › [jose](../jose.md) › [jwk](README.md)

# sgcl::crypto::jose::jwk::to_json

```cpp
string to_json() const;
```

The public JWK as compact JSON: [parameters](parameters.md) written out. Of a private key, its public half: what an
issuer publishes.

## Parameters

None.

## Return value

The JSON.

## Complexity

Linear in the size of the key.

## Exceptions

`std::logic_error` for an oct key, which has no public form ([to_private_json](to_private_json.md) writes it).

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto key = crypto::jose::jwk::parse(R"({"kty":"OKP","crv":"Ed25519","kid":"a",
        "d":"nWGxne_9WmC6hEr0kuwsxERJxWl7MmkZcDusAxyuf2A","x":"11qYAYKxCrfVS_7TyWQHOg7hcvPapiMlrwIaaPcHURo"})");
    println("{}", key->to_json());
}
```

Output:

```text
{"kty":"OKP","crv":"Ed25519","kid":"a","x":"11qYAYKxCrfVS_7TyWQHOg7hcvPapiMlrwIaaPcHURo"}
```

## See also

- [parse](parse.md): the other way
- [sgcl::crypto::jose::jwk](README.md)
