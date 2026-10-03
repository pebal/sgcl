[sgcl](../../README.md) › [crypto](../README.md) › [secret_bytes](../secret_bytes.md)

# sgcl::crypto::secret_bytes::clone

```cpp
secret_bytes clone() const noexcept;
```

Returns a second secret of the same bytes, in its own inline bytes or its own block of plain memory. A
`secret_bytes` has no copy constructor, so that every copy of it is one the program asked for by name: one more place
the bytes lie, zeroed when that secret goes in turn.

## Parameters

None.

## Return value

A secret of the same bytes.

## Complexity

Linear in [size](size.md).

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    crypto::secret_bytes key = crypto::random::secret(100);
    crypto::secret_bytes copy = key.clone();
    println("{} {}", copy == key, copy.as_slice().data() != key.as_slice().data());
}
```

Output:

```text
true true
```

## See also

- [(constructor)](secret_bytes.md): the move
- [sgcl::crypto::secret_bytes](../secret_bytes.md)
