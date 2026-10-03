[sgcl](../../README.md) › [crypto](../README.md) › [aes_ctr](README.md)

# sgcl::crypto::aes_ctr::xor_key_stream

```cpp
void xor_key_stream(const slice<byte>& out, const slice<const byte>& in);
```

XORs `in` with the next `in.size()` bytes of the keystream into `out`: encrypts a plaintext, or decrypts a
ciphertext, the same call. Go's `stream.XORKeyStream(dst, src)`. The calls continue one another at any length:
10 bytes and then 20 are the same as 30 at once, the unused part of a block kept for the next call. The counter
wraps at 2^128.

`out` may be `in` itself, beginning at its first byte: the data is then encrypted in place.

## Parameters

| Parameter | Description |
|---|---|
| `out` | where the result goes: at least `in.size()` bytes, `in` itself or apart from it |
| `in` | the plaintext or the ciphertext |

## Return value

None.

## Complexity

Linear in `in.size()`.

## Exceptions

- `length_error` when `out` is shorter than `in`.
- `invalid_argument` when `out` overlaps `in` other than by beginning at the same byte.
- `logic_error` when the object was moved from and holds no key.

Nothing is written and the place in the keystream does not move then.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // SP 800-38A F.5.1, in place, in two calls of 10 and 54 bytes
    vector<byte> key = encoding::hex::decode("2b7e151628aed2a6abf7158809cf4f3c");
    vector<byte> iv = encoding::hex::decode("f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff");
    vector<byte> data = encoding::hex::decode(
        "6bc1bee22e409f96e93d7e117393172aae2d8a571e03ac9c9eb76fac45af8e51"
        "30c81c46a35ce411e5fbc1191a0a52eff69f2445df4f9b17ad2b417be66c3710");
    crypto::aes_ctr ctr(key, iv);
    ctr.xor_key_stream(data.as_slice(0, 10), data.as_slice(0, 10));
    ctr.xor_key_stream(data.as_slice(10), data.as_slice(10));
    println("{}", encoding::hex::encode(data.as_slice(0, 32)));
    println("{}", encoding::hex::encode(data.as_slice(32)));

    try {
        ctr.xor_key_stream(data.as_slice(0, 4), data);
    } catch (const length_error& e) {
        println("{}", e.what());
    }
}
```

Output:

```text
874d6191b620e3261bef6864990db6ce9806f66b7970fdff8617187bb9fffdff
5ae4df3edbd5d35e5b4f09020db03eab1e031dda2fbe03d1792170a0f3009cee
sgcl::crypto::aes_ctr::xor_key_stream: the output is shorter than the input
```

## See also

- [seek](seek.md): moves to the start of a block
- [sgcl::crypto::aes_ctr](README.md)
