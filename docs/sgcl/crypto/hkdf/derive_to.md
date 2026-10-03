[sgcl](../../README.md) › [crypto](../README.md) › [hkdf](README.md)

# sgcl::crypto::hkdf\<H\>::derive_to

```cpp
static void derive_to(const slice<byte>& out, const slice<const byte>& salt,
                      const slice<const byte>& ikm, const slice<const byte>& info);
```

[derive](derive.md) into a buffer of the caller's: `out.size()` bytes from the input keying material `ikm` under
`salt`, bound to `info`, written into `out` with no allocation. The PRK between the two steps is zeroed before the
call returns; the buffer is the caller's to clear ([secure_zero](../secure_zero.md)).

`info` is read again for every block, so `out` may not overlap it; `salt` and `ikm` are read whole before the first
byte is written, so `out` may lie over them.

## Parameters

| Parameter | Description |
|---|---|
| `out` | the buffer to fill, at most `max_size` bytes |
| `salt` | a value both sides know, not a secret; may be empty |
| `ikm` | the input keying material |
| `info` | what the output is for; may be empty |

## Return value

None.

## Complexity

Linear in the lengths of `salt` and `ikm`, and in `out.size()`.

## Exceptions

`std::invalid_argument` when `out.size()` is greater than `max_size`, 255 blocks of the digest, or when `out`
overlaps `info`; nothing is computed.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // RFC 5869, test case 1
    vector<byte> ikm(22, byte(0x0b));
    auto salt = encoding::hex::decode("000102030405060708090a0b0c");
    auto info = encoding::hex::decode("f0f1f2f3f4f5f6f7f8f9");
    byte okm[42];
    crypto::hkdf_sha256::derive_to(okm, salt, ikm, info);
    println(encoding::hex::encode(okm));
    crypto::secure_zero(okm);
}
```

Output:

```text
3cb25f25faacd57a90434f64d0362f2a2d2d0a90cf1a5a4c5db02d56ecc4c5bf34007208d5b887185865
```

## See also

- [derive](derive.md): the bytes as a `secret_bytes`
- [expand_to](expand_to.md): the second step into a buffer
- [sgcl::crypto::hkdf\<H\>](README.md)
