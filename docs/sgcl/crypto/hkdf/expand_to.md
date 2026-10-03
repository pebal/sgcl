[sgcl](../../README.md) › [crypto](../README.md) › [hkdf](README.md)

# sgcl::crypto::hkdf\<H\>::expand_to

```cpp
static void expand_to(const slice<byte>& out, const prk& key,                  // (1)
                      const slice<const byte>& info);
static void expand_to(const slice<byte>& out, const slice<const byte>& key,    // (2)
                      const slice<const byte>& info);
```

[expand](expand.md) into a buffer of the caller's: `out.size()` bytes of output keying material from the PRK, bound
to `info`, written into `out` with no allocation — a stack array, a key object's own storage, for keys that must not
linger. (1) takes the PRK [extract](extract.md) gave, (2) a PRK as bytes. The buffer is the caller's to clear
([secure_zero](../secure_zero.md)); the last block computed is zeroed before the call returns.

`info` is read again for every block, so `out` may not overlap it; the key is read whole before the first byte is
written, so `out` may lie over a key given as bytes.

## Parameters

| Parameter | Description |
|---|---|
| `out` | the buffer to fill, at most `max_size` bytes |
| `key` | the pseudorandom key |
| `info` | what the output is for; may be empty |

## Return value

None.

## Complexity

Linear in `out.size()`: one HMAC of a block for every `H::digest_size` bytes.

## Exceptions

`std::invalid_argument` when `out.size()` is greater than `max_size`, 255 blocks of the digest, or when `out`
overlaps `info`; nothing is written.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // RFC 5869, test case 3: no salt, no info
    vector<byte> ikm(22, byte(0x0b));
    auto master = crypto::hkdf_sha256::extract("", ikm);
    byte okm[42];
    crypto::hkdf_sha256::expand_to(okm, master, "");
    println(encoding::hex::encode(okm));
    crypto::secure_zero(okm);
}
```

Output:

```text
8da4e775a563c18f715f802a063c5a31b8a11f5c5ee1879ec3454e5f3c738d2d9d201395faa4b61a96c8
```

## See also

- [expand](expand.md): the bytes as a `secret_bytes`
- [derive_to](derive_to.md): both steps into a buffer
- [sgcl::crypto::hkdf\<H\>](README.md)
