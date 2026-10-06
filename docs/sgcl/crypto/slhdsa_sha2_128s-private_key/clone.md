[sgcl](../../README.md) › [crypto](../README.md) › [slhdsa_sha2_128s](../slhdsa.md) › [private_key](README.md)

# sgcl::crypto::slhdsa_sha2_128s::private_key::clone

```cpp
private_key clone() const;
```

A second key of the same bytes: the private key is move-only, so a second one is made by name.

## Parameters

None.

## Return value

The key.

## Complexity

Constant.

## Exceptions

`std::logic_error` when the key was moved from.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto key = crypto::slhdsa_sha2_128s::private_key::generate();
    println("{}", key.clone() == key);
}
```

Output:

```text
true
```

## See also

- [operator==](operator_cmp.md)
- [sgcl::crypto::slhdsa_sha2_128s::private_key](README.md)
