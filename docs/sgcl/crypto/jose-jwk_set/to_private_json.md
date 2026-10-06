[sgcl](../../README.md) › [crypto](../README.md) › [jose](../jose.md) › [jwk_set](README.md)

# sgcl::crypto::jose::jwk_set::to_private_json

```cpp
secret_bytes to_private_json() const;
```

The set's JSON with every key's private members ([jwk::to_private_json](../jose-jwk/to_private_json.md)), oct keys
included, in plain memory: a [secret_bytes](../secret_bytes/README.md), never managed memory.

## Parameters

None.

## Return value

The JSON, in a secret_bytes.

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
    crypto::jose::jwk_set set{crypto::jose::jwk::symmetric("0123456789abcdef", {.kid = "k"})};
    auto json = set.to_private_json();
    println("{}", string(json));
}
```

Output:

```text
{"keys":[{"kty":"oct","kid":"k","k":"MDEyMzQ1Njc4OWFiY2RlZg"}]}
```

## See also

- [parse](parse.md): the other way
- [sgcl::crypto::jose::jwk_set](README.md)
