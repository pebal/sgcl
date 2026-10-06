[sgcl](../../README.md) › [crypto](../README.md) › [jose](../jose.md) › [jwk](README.md)

# sgcl::crypto::jose::jwk::type

```cpp
key_type type() const noexcept;
```

The kind of the key, its `kty`.

## Parameters

None.

## Return value

`key_type::ec`, `rsa`, `oct` or `okp` ([key_type](../jose-key_type.md)).

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
    auto key = crypto::jose::jwk::generate(crypto::jose::algorithm::rs256);
    println("{}", key.type() == crypto::jose::key_type::rsa);
}
```

Output:

```text
true
```

## See also

- [crv](crv.md): the curve of an ec or okp key
- [sgcl::crypto::jose::jwk](README.md)
