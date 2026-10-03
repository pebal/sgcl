[sgcl](../../README.md) › [crypto](../README.md) › [secret_bytes](../secret_bytes.md)

# sgcl::crypto::secret_bytes::secret_bytes

```cpp
secret_bytes() noexcept = default;                  // (1)
explicit secret_bytes(size_t n) noexcept;           // (2)
secret_bytes(secret_bytes&&) noexcept = default;    // (3)
secret_bytes(const secret_bytes&) = delete;         // (4)
```

1. Empty. Nothing is allocated.
2. `n` bytes, all zero, to be written through [as_slice](as_slice.md): up to 64 in the object itself, past that in a
   block of plain memory of exactly `n`.
3. Takes the bytes of the other secret over: a block is handed on, inline bytes are copied and zeroed in the other
   secret. The other secret is left empty.
4. A secret is not copied by a constructor: a copy is asked for by name, with [clone](clone.md).

The destructor zeroes what the secret held, with stores the compiler cannot drop, and frees its block.

## Parameters

| Parameter | Description |
|---|---|
| `n` | the number of bytes |

## Complexity

- (1), (3) Constant; (3) linear in the inline bytes, at most 64.
- (2) Linear in `n`.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    crypto::secret_bytes none;
    crypto::secret_bytes key(32);
    crypto::random::fill(key);

    crypto::secret_bytes moved(std::move(key));
    println("{} {} {}", none.empty(), key.empty(), moved.size());
}
```

Output:

```text
true true 32
```

## See also

- [operator=](operator_assign.md): the move by an assignment
- [clone](clone.md): a copy by name
- [resize](resize.md): changes the number of bytes
- [sgcl::crypto::secret_bytes](../secret_bytes.md)
