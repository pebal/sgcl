[sgcl](../../README.md) › [crypto](../README.md) › [jose](../jose.md) › [jws](README.md)

# sgcl::crypto::jose::jws::alg

```cpp
optional<algorithm> alg(size_t i = 0) const noexcept;
```

The algorithm the header of signature `i` names. Not the one a verification uses: that is the key's
([verify](verify.md)).

## Parameters

| Parameter | Description |
|---|---|
| `i` | the signature, 0 by default |

## Return value

The algorithm; `nullopt` for a name the module does not have (`none` among them), and for an `i` past the last
signature.

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
    println("{}", j->alg() == crypto::jose::algorithm::hs256);
}
```

Output:

```text
true
```

## See also

- [algorithm](../jose-algorithm.md)
- [sgcl::crypto::jose::jws](README.md)
