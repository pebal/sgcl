[sgcl](../../README.md) › [crypto](../README.md) › [aes_kw](README.md)

# sgcl::crypto::aes_kw::wrap

```cpp
vector<byte> wrap(const slice<const byte>& key) const;
```

Wraps `key` by RFC 3394's algorithm: the register `A6A6A6A6A6A6A6A6` and the key's 64-bit halves through six passes of
AES, the register written first. The key must be 16 bytes or more and a multiple of 8, as RFC 3394 has it;
[wrap_padded](wrap_padded.md) takes any length.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key to wrap, 8 n bytes with n of 2 or more |

## Return value

The wrapped key, `key.size() + 8` bytes; not a secret.

## Complexity

Linear in `key.size()`: 6 n AES encryptions.

## Exceptions

`invalid_argument` when `key` is shorter than 16 bytes or not a multiple of 8; `logic_error` when the object was moved
from.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // RFC 3394 §4.6: 256 bits of key under a 256-bit key-encryption key
    vector<byte> kek = encoding::hex::decode("000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f");
    vector<byte> key = encoding::hex::decode("00112233445566778899aabbccddeeff000102030405060708090a0b0c0d0e0f");
    println(encoding::hex::encode(crypto::aes_kw(kek).wrap(key)));
}
```

Output:

```text
28c9f404c4b810f4cbccb35cfb87f8263f5786e2d80ed326cbc7f0e71a99f43bfb988b9b7a02dd21
```

## See also

- [unwrap](unwrap.md): the key back
- [wrap_padded](wrap_padded.md): a key of any length
- [sgcl::crypto::aes_kw](README.md)
