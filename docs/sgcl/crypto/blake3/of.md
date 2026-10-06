[sgcl](../../README.md) › [crypto](../README.md) › [blake3](README.md)

# sgcl::crypto::blake3::of

```cpp
static array<byte, 32> of(const slice<const byte>& data) noexcept;                         // (1)
static array<byte, 32> of(const slice<const byte>& data, const slice<const byte>& key);    // (2)
```

The output's first 32 bytes over `data` in one call.

1. The hash, the mixin's form ([hash::mixin::hasher](../../hash/mixin/hasher/README.md)): bytes or text, and the
   other forms `update` takes.
2. The keyed hash under `key`, 32 bytes: the data first and the key after it, as `hmac::of(data, key)`. The state
   made inside is zeroed before the call returns.

## Parameters

| Parameter | Description |
|---|---|
| `data` | the input, bytes or text |
| `key` | the secret key, 32 bytes |

## Return value

32 bytes of output: the hash, or the MAC.

## Complexity

Linear in `data.size()`.

## Exceptions

- (1) None.
- (2) `std::invalid_argument` when `key` is not 32 bytes long.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    println(encoding::hex::encode(crypto::blake3::of("abc")));
    println(encoding::hex::encode(crypto::blake3::of("message", "0123456789abcdef0123456789abcdef")));
}
```

Output:

```text
6437b3ac38465133ffb63b75273a8db548c558465d79db03fd359c6cd5bd9d85
0375fb06e058b29b01dda52184b59c3fb1cbf0230e4d45168c6c28928f6b8577
```

## See also

- [(constructor)](blake3.md): a hasher for an input in pieces
- [derive_key](derive_key.md): the third mode in one call
- [sgcl::crypto::blake3](README.md)
