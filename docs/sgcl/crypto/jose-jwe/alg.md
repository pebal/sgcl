[sgcl](../../README.md) › [crypto](../README.md) › [jose](../jose.md) › [jwe](README.md)

# sgcl::crypto::jose::jwe::alg

```cpp
optional<algorithm> alg() const noexcept;
```

The key management the header names.

## Parameters

None.

## Return value

The algorithm; `nullopt` for one the module does not have.

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
    println("{}", j->alg() == crypto::jose::algorithm::a128gcmkw);
}
```

Output:

```text
true
```

## See also

- [algorithm](../jose-algorithm.md)
- [sgcl::crypto::jose::jwe](README.md)
