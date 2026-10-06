[sgcl](../../README.md) › [crypto](../README.md) › [jose](../jose.md) › [jws](README.md)

# sgcl::crypto::jose::jws::header

```cpp
encoding::json header(size_t i = 0) const noexcept;
```

The header of signature `i`: its protected header and its unprotected one (of a JSON form) as one object, the
protected members first. Not verified until [verify](verify.md) says so.

## Parameters

| Parameter | Description |
|---|---|
| `i` | the signature, 0 by default |

## Return value

The header; null for an `i` past the last signature.

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
    println("{}", j->header().to_string());
    println("{}", j->header(1).is_null());
}
```

Output:

```text
{"alg":"HS256","kid":"s1"}
true
```

## See also

- [alg](alg.md), [kid](kid.md)
- [sgcl::crypto::jose::jws](README.md)
