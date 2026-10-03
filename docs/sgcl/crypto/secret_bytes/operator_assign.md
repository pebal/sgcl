[sgcl](../../README.md) › [crypto](../README.md) › [secret_bytes](README.md)

# sgcl::crypto::secret_bytes::operator=

```cpp
secret_bytes& operator=(secret_bytes&&) noexcept = default;    // (1)
secret_bytes& operator=(const secret_bytes&) = delete;         // (2)
```

1. Zeroes and lets go of what this secret held, and takes the bytes of the other over: a block is handed on, inline
   bytes are copied and zeroed in the other secret. The other secret is left empty.
2. A secret is not copied by an assignment: a copy is asked for by name, with [clone](clone.md).

## Parameters

| Parameter | Description |
|---|---|
| `other` | the secret whose bytes are taken over |

## Return value

`*this`.

## Complexity

Linear in the inline bytes of both, at most 64 each.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    crypto::secret_bytes key = crypto::random::secret(32);
    crypto::secret_bytes next = crypto::random::secret(100);
    key = std::move(next);
    println("{} {}", key.size(), next.empty());
}
```

Output:

```text
100 true
```

## See also

- [(constructor)](secret_bytes.md): the move by a construction
- [clone](clone.md): a copy by name
- [sgcl::crypto::secret_bytes](README.md)
