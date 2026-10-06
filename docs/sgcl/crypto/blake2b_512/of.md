[sgcl](../../README.md) › [crypto](../README.md) › [blake2b_512](README.md)

# sgcl::crypto::blake2b_512::of

```cpp
static array<byte, digest_size> of(const slice<const byte>& data) noexcept;                    // (1)
static array<byte, digest_size> of(const slice<const byte>& data, const blake2_options& o);    // (2)
```

The digest of `data` in one call: a hasher made, updated with the data, asked its value.

1. The plain digest, the mixin's form ([hash::mixin::hasher](../../hash/mixin/hasher/README.md)): bytes or text, which
   the slice takes both, and the other forms `update` takes.
2. With the key, the salt and the personalization of `o`: the data first and the options after it, as every keyed
   type of the module has it (`hmac::of(data, key)`). The state made inside is zeroed before the call returns when
   it is keyed.

## Parameters

| Parameter | Description |
|---|---|
| `data` | the message, bytes or text |
| `o` | the key, the salt and the personalization ([blake2_options](../blake2_options.md)) |

## Return value

The digest, `digest_size` bytes; with a key, the MAC.

## Complexity

Linear in `data.size()`.

## Exceptions

- (1) None.
- (2) `std::invalid_argument` when the key, the salt or the personalization is longer than its field.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    println(encoding::hex::encode(crypto::blake2b_256::of("abc")));
    println(encoding::hex::encode(crypto::blake2b_256::of("hello world", {.key = "k"})));
}
```

Output:

```text
bddd813c634239723171ef3fee98579b94964e3bb1cb3e427262c8c068d52319
94d9f21e168de024709e8dea7e2424e2c049404344b94963ee24a473d9046d33
```

## See also

- [(constructor)](blake2b_512.md): a hasher for a message in pieces
- [verify](verify.md): checks a received tag
- [sgcl::crypto::blake2b_512](README.md)
