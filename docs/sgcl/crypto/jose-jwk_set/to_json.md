[sgcl](../../README.md) › [crypto](../README.md) › [jose](../jose.md) › [jwk_set](README.md)

# sgcl::crypto::jose::jwk_set::to_json

```cpp
string to_json() const;
```

The set's JSON, `{"keys":[…]}`, compact, with the public JWK of every key ([jwk::to_json](../jose-jwk/to_json.md)):
what an issuer publishes. The oct keys, which have no public form, are left out.

## Parameters

None.

## Return value

The JSON.

## Complexity

Linear in the size of the keys.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto key = crypto::jose::jwk::parse(R"({"kty":"OKP","crv":"Ed25519","kid":"a",
        "d":"nWGxne_9WmC6hEr0kuwsxERJxWl7MmkZcDusAxyuf2A","x":"11qYAYKxCrfVS_7TyWQHOg7hcvPapiMlrwIaaPcHURo"})");
    crypto::jose::jwk_set set{*key, crypto::jose::jwk::generate(crypto::jose::algorithm::hs256)};
    println("{}", set.to_json());
}
```

Output:

```text
{"keys":[{"kty":"OKP","crv":"Ed25519","kid":"a","x":"11qYAYKxCrfVS_7TyWQHOg7hcvPapiMlrwIaaPcHURo"}]}
```

## See also

- [parse](parse.md): the other way
- [sgcl::crypto::jose::jwk_set](README.md)
