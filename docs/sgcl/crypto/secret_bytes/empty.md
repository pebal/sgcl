[sgcl](../../README.md) › [crypto](../README.md) › [secret_bytes](README.md)

# sgcl::crypto::secret_bytes::empty

```cpp
bool empty() const noexcept;
```

Checks whether the secret has no bytes: one constructed empty, or moved from.

## Parameters

None.

## Return value

`true` when [size](size.md) is 0.

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
    crypto::secret_bytes key = crypto::random::secret(32);
    println("{}", key.empty());
    crypto::secret_bytes taken = std::move(key);
    println("{}", key.empty());
}
```

Output:

```text
false
true
```

## See also

- [size](size.md): the number of bytes
- [sgcl::crypto::secret_bytes](README.md)
