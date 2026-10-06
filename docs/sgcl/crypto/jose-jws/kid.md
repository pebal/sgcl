[sgcl](../../README.md) › [crypto](../README.md) › [jose](../jose.md) › [jws](README.md)

# sgcl::crypto::jose::jws::kid

```cpp
string kid(size_t i = 0) const noexcept;
```

The `kid` of the header of signature `i`: the key to verify it with, as a [key set](../jose-jwk_set/README.md) finds it.

## Parameters

| Parameter | Description |
|---|---|
| `i` | the signature, 0 by default |

## Return value

The kid; empty when the header has none, and for an `i` past the last signature.

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
    auto key = crypto::jose::jwk::symmetric("a secret of thirty-two bytes ...", {.kid = "s1"});
    auto token = crypto::jose::jws::sign_json("hello", crypto::jose::jwk_set{key});
    auto j = crypto::jose::jws::parse(token);
    println("{}", j->kid());
}
```

Output:

```text
s1
```

## See also

- [jwk_set::find](../jose-jwk_set/find.md)
- [sgcl::crypto::jose::jws](README.md)
