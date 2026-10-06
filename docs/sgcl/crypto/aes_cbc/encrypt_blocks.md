[sgcl](../../README.md) › [crypto](../README.md) › [aes_cbc](README.md)

# sgcl::crypto::aes_cbc::encrypt_blocks

```cpp
void encrypt_blocks(const slice<byte>& out, const slice<const byte>& in);
```

Encrypts the whole blocks of `in` into `out`, the chain carried on from the last call and on to the next: Go's
`CryptBlocks`, for a format that pads its data itself, or a stream encrypted in pieces. `out` holds at least
`in.size()` bytes and may be `in` itself.

## Parameters

| Parameter | Description |
|---|---|
| `out` | where the ciphertext goes, at least `in.size()` bytes |
| `in` | whole blocks of plaintext |

## Return value

None.

## Complexity

Linear in `in.size()`; one block after another, each waiting for the one before.

## Exceptions

- `invalid_argument` when `in.size()` is not a multiple of 16, or `out` overlaps `in` other than exactly.
- `length_error` when `out` is shorter than `in`.
- `logic_error` when the object was moved from.

Nothing is written when one is thrown.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // SP 800-38A F.2.1, the first two blocks, one call each
    vector<byte> key = encoding::hex::decode("2b7e151628aed2a6abf7158809cf4f3c");
    vector<byte> iv = encoding::hex::decode("000102030405060708090a0b0c0d0e0f");
    vector<byte> text = encoding::hex::decode("6bc1bee22e409f96e93d7e117393172aae2d8a571e03ac9c9eb76fac45af8e51");
    crypto::aes_cbc cbc(key, iv);
    cbc.encrypt_blocks(text.as_slice(0, 16), text.as_slice(0, 16));
    cbc.encrypt_blocks(text.as_slice(16), text.as_slice(16));
    println(encoding::hex::encode(text));
}
```

Output:

```text
7649abac8119b246cee98e9b12e9197d5086cb9b507219ee95db113a917678b2
```

## See also

- [decrypt_blocks](decrypt_blocks.md): the other way
- [encrypt](encrypt.md): a message with its padding
- [sgcl::crypto::aes_cbc](README.md)
