[sgcl](../../README.md) › [crypto](../README.md) › [jose](../jose.md) › [jwe](README.md)

# sgcl::crypto::jose::jwe::header

```cpp
encoding::json header() const noexcept;
```

The protected header: `alg`, `enc`, `kid`, what the algorithm adds (`epk`, `iv`, `tag`) and the sender's own members.
Authenticated by the tag, so trusted only after [decrypt](decrypt.md).

## Parameters

None.

## Return value

The header, an object.

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
    println("{}", j->header()["kid"].as_string("?"));
}
```

Output:

```text
k7
```

## See also

- [alg](alg.md), [enc](enc.md)
- [sgcl::crypto::jose::jwe](README.md)
