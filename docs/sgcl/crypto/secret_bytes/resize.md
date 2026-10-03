[sgcl](../../README.md) › [crypto](../README.md) › [secret_bytes](../secret_bytes.md)

# sgcl::crypto::secret_bytes::resize

```cpp
void resize(size_t n) noexcept;
```

Makes the secret `n` bytes long: the first `min(n, size())` are kept, the rest are zero. Past the capacity the bytes
move to a new block of plain memory of exactly `n`, and the block, or the inline bytes, they leave is zeroed. A
shrink zeroes the bytes it drops and keeps the room. So a secret read in pieces of unknown total, as
[read_secret](../read_secret.md) reads a file, leaves no copy behind it as it grows.

## Parameters

| Parameter | Description |
|---|---|
| `n` | the new number of bytes |

## Return value

None.

## Complexity

Linear in `n` and in the bytes kept.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    crypto::secret_bytes key = crypto::random::secret(32);
    crypto::secret_bytes copy = key.clone();

    key.resize(100);  // past the 64 inline bytes
    array<byte, 68> zeros{};
    bool kept = crypto::constant_time::equal(key.as_slice().subslice(0, 32), copy);
    println("{} {}", key.size(), kept);
    println("{}", crypto::constant_time::equal(key.as_slice().subslice(32), zeros));

    key.resize(16);
    kept = crypto::constant_time::equal(key, copy.as_slice().subslice(0, 16));
    println("{} {}", key.size(), kept);
}
```

Output:

```text
100 true
true
16 true
```

## See also

- [size](size.md): the number of bytes
- [sgcl::crypto::secret_bytes](../secret_bytes.md)
