[sgcl](../../README.md) › [crypto](../README.md) › [jose](../jose.md) › [jwe](README.md)

# sgcl::crypto::jose::jwe::enc

```cpp
optional<encryption> enc() const noexcept;
```

The content encryption the header names.

## Parameters

None.

## Return value

The encryption; `nullopt` for one the module does not have.

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
    auto key = crypto::jose::jwk::generate(crypto::jose::algorithm::a128gcmkw, {.kid = "k7"});
    auto j = crypto::jose::jwe::parse(crypto::jose::jwe::encrypt("x", key));
    println("{}", j->enc() == crypto::jose::encryption::a256gcm);
}
```

Output:

```text
true
```

## See also

- [encryption](../jose-encryption.md)
- [sgcl::crypto::jose::jwe](README.md)
