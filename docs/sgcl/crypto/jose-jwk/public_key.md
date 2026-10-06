[sgcl](../../README.md) › [crypto](../README.md) › [jose](../jose.md) › [jwk](README.md)

# sgcl::crypto::jose::jwk::public_key

```cpp
jwk public_key() const;
```

The key's public half, with the same members: what verifies its signatures and what is encrypted to.

## Parameters

None.

## Return value

The public key.

## Complexity

Linear in the size of the key.

## Exceptions

`std::logic_error` for an oct key, which has no public half.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto key = crypto::jose::jwk::generate(crypto::jose::algorithm::es256);
    auto token = crypto::jose::jws::sign("hello", key);
    println("{}", crypto::jose::jws::verify(token, key.public_key()).has_value());
}
```

Output:

```text
true
```

## See also

- [is_private](is_private.md)
- [sgcl::crypto::jose::jwk](README.md)
