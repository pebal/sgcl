[sgcl](../README.md) › [crypto](README.md)

# sgcl::crypto::digest

```cpp
#include "sgcl/crypto/hash_id.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto {
    vector<byte> digest(hash_id id, const slice<const byte>& data);
}
```

The digest of `data` by the algorithm `id` names: `sha256::of(data)` when `id` is `hash_id::sha256`, as a
`vector<byte>` of `digest_size(id)` bytes, for where the algorithm comes from data — a certificate, a handshake, a
key's parameters. The data is bytes or text, which the slice takes both. Where the digest is known in the code, its
type's `of(data)` is simpler and gives an `array` with no allocation.

## Parameters

| Parameter | Description |
|---|---|
| `id` | the digest, one of the values of [hash_id](hash_id.md) |
| `data` | the bytes to hash, or a text |

## Return value

The digest, `digest_size(id)` bytes.

## Complexity

Linear in `data.size()`.

## Exceptions

`std::invalid_argument` when `id` is none of the values of `hash_id` (a number cast to it).

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto id = crypto::hash_id::sha384;  // as a certificate names it
    vector<byte> d = digest(id, "abc");
    println("{} {}", d.size(), encoding::hex::encode(d));
    println("{}", encoding::hex::encode(d) == encoding::hex::encode(crypto::sha384::of("abc")));
}
```

Output:

```text
48 cb00753f45a35e8bb5a03d699ac65007272c32ab0eded1631a8b605a43ff5bed8086072ba1e7cc2358baeca134c825a7
true
```

## See also

- [digest_file](digest_file.md): a whole file, read a block at a time
- [digest_size](digest_size.md): the length of what it gives
- [sha256](sha256.md), [sha512](sha512.md), [sha3_256](sha3_256.md): the digests by their types
- [sgcl::crypto::hash_id](hash_id.md)
