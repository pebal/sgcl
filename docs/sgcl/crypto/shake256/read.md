[sgcl](../../README.md) › [crypto](../README.md) › [shake256](../shake256.md)

# sgcl::crypto::shake256::read

```cpp
secret_bytes read(size_t n) noexcept;
```

The next `n` bytes of the output. The first read pads the input (the domain bits `1111` of SHAKE and the pad of
FIPS 202) and closes it; every read goes on where the last one stopped, so two reads of 16 bytes give what one read
of 32 gives. The bytes are taken as a secret, since SHAKE derives keys as often as not: a
[secret_bytes](../secret_bytes.md), up to 64 bytes in the object itself, past that in plain memory zeroed when it goes,
never in managed memory. [read_to](read_to.md) writes into a buffer of the caller's instead.

## Parameters

| Parameter | Description |
|---|---|
| `n` | the number of bytes to read, any |

## Return value

The `n` bytes.

## Complexity

Linear in `n`: one permutation a block of the rate.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    crypto::shake128 x;
    x.update("seed");
    auto first = x.read(16);
    auto next = x.read(16);
    println(encoding::hex::encode(first));
    println(encoding::hex::encode(next));
    println(encoding::hex::encode(crypto::shake128::of("seed", 32)));
}
```

Output:

```text
25629347589242761d31f826ba4b757b
4f4f95668c83dfb6401762bb2d01a262
25629347589242761d31f826ba4b757b4f4f95668c83dfb6401762bb2d01a262
```

## See also

- [read_to](read_to.md): into a buffer of the caller's
- [of](of.md): the first bytes in one call
- [sgcl::crypto::shake256](../shake256.md)
