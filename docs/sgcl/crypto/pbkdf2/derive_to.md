[sgcl](../../README.md) › [crypto](../README.md) › [pbkdf2](README.md)

# sgcl::crypto::pbkdf2\<H\>::derive_to

```cpp
static void derive_to(const slice<byte>& out, const slice<const byte>& password,
                      const slice<const byte>& salt, uint32_t iterations);
```

[derive](derive.md) into a buffer of the caller's: `out.size()` bytes derived from `password` and `salt` in
`iterations` rounds, written into `out` with no allocation, for a key that must not linger. The buffer is the
caller's to clear when done ([secure_zero](../secure_zero.md)); the keyed states and every intermediate block are
zeroed before the call returns.

`salt` is read again for every block, so `out` may not overlap it; the password is read whole before the first byte
is written, so `out` may lie over it.

## Parameters

| Parameter | Description |
|---|---|
| `out` | the buffer to fill, at most 2^32 − 1 blocks of `H::digest_size` |
| `password` | the password, bytes or text |
| `salt` | the salt, random and stored beside the result |
| `iterations` | the rounds per block, at least 1: the work factor |

## Return value

None.

## Complexity

`iterations` HMACs for every `H::digest_size` bytes of `out`.

## Exceptions

`std::invalid_argument` when `iterations` is 0, when `out.size()` is more than 2^32 − 1 blocks of the digest's
size, or when `out` overlaps `salt`; nothing is written.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

#include <string_view>

using namespace sgcl;

int main() {
    // RFC 6070, HMAC-SHA-1: a password and a salt with a NUL inside
    std::string_view password("pass\0word", 9);
    std::string_view salt("sa\0lt", 5);
    byte key[16];
    crypto::pbkdf2<crypto::sha1>::derive_to(key, password, salt, 4096);
    println(encoding::hex::encode(key));
    crypto::secure_zero(key);
}
```

Output:

```text
56fa6aa75548099dcc37d7f03425e0c3
```

## See also

- [derive](derive.md): the bytes as a `secret_bytes`
- [secure_zero](../secure_zero.md): clears the buffer after use
- [sgcl::crypto::pbkdf2\<H\>](README.md)
