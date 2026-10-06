[sgcl](../../README.md) › [crypto](../README.md) › [jose](../jose.md) › [jwe](README.md)

# sgcl::crypto::jose::jwe::kid

```cpp
string kid() const noexcept;
```

The `kid` of the header: the recipient's key, as a [key set](../jose-jwk_set/README.md) finds it.

## Parameters

None.

## Return value

The kid; empty when the header has none.

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
    println("{}", j->kid());
}
```

Output:

```text
k7
```

## See also

- [jwk::kid](../jose-jwk/kid.md)
- [sgcl::crypto::jose::jwe](README.md)
