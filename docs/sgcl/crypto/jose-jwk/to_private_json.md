[sgcl](../../README.md) › [crypto](../README.md) › [jose](../jose.md) › [jwk](README.md)

# sgcl::crypto::jose::jwk::to_private_json

```cpp
secret_bytes to_private_json() const;
```

The JWK as compact JSON with its private members (`d`; RSA's `d`, `p`, `q`, `dp`, `dq`, `qi`; an oct key's `k`) after
the public ones, written in plain memory: a [secret_bytes](../secret_bytes/README.md), never managed memory, for
[io::write_file](../../io/write_file.md) with permissions 0600. Of a public key, the same as [to_json](to_json.md).

## Parameters

None.

## Return value

The JSON, in a secret_bytes.

## Complexity

Linear in the size of the key.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto key = crypto::jose::jwk::symmetric("0123456789abcdef");
    auto json = key.to_private_json();
    println("{}", string(json));
}
```

Output:

```text
{"kty":"oct","k":"MDEyMzQ1Njc4OWFiY2RlZg"}
```

## See also

- [parse](parse.md): the other way
- [sgcl::crypto::jose::jwk](README.md)
