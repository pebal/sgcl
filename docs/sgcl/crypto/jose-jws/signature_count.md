[sgcl](../../README.md) › [crypto](../README.md) › [jose](../jose.md) › [jws](README.md)

# sgcl::crypto::jose::jws::signature_count

```cpp
size_t signature_count() const noexcept;
```

The number of signatures: one for the compact and the flattened form, one or more for the general form.

## Parameters

None.

## Return value

The number.

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
    println("{}", j->signature_count());
}
```

Output:

```text
1
```

## See also

- [header](header.md)
- [sgcl::crypto::jose::jws](README.md)
