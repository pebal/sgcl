[sgcl](../../README.md) › [hash](../README.md) › [siphash](../siphash.md)

# sgcl::hash::siphash::siphash

```cpp
explicit siphash(const array<byte, 16>& key) noexcept;
```

Makes a hasher of no bytes yet under `key`: sixteen bytes, read as two little-endian words as the paper and the
reference implementation read them, so a key written down as bytes gives the reference's values. Go's
`siphash.New(key)` of `dchest/siphash`.

There is no hasher without a key. A program makes one once from a source of randomness,
[crypto::random](../../crypto/random.md), and keeps it secret, for as long as the table it hashes lives.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key, sixteen bytes |

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/hash.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    array<byte, 16> key;
    crypto::random::fill(key);

    hash::siphash h(key);
    h.update("hello");
    println("{}", h.value() == hash::siphash::of("hello", key));
}
```

Output:

```text
true
```

## See also

- [of](../mixin/hasher/of.md): the hash under a key in one call
- [sgcl::hash::siphash](../siphash.md)
