[sgcl](../../README.md) › [crypto](../README.md) › [chacha20](../chacha20.md)

# sgcl::crypto::chacha20::xor_key_stream

```cpp
void xor_key_stream(const slice<byte>& out, const slice<const byte>& in);
```

XORs `in` with the next `in.size()` bytes of the keystream into `out`: encrypts a plaintext, or decrypts a
ciphertext, the same call. Go's `c.XORKeyStream(dst, src)`. The calls continue one another at any length: 10
bytes and then 20 are the same as 30 at once, the unused part of a block kept for the next call.

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

- `length_error` when `out` is shorter than `in`, or when the call would need a block past the last of the 2^32
  blocks (256 GiB) of the keystream, as Go panics.
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
    // RFC 8439 §2.4.2 from block 1, in two calls of 10 and 30 bytes
    vector<byte> key =
        encoding::hex::decode("000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f");
    vector<byte> nonce = encoding::hex::decode("000000000000004a00000000");
    string text = "Ladies and Gentlemen of the class of '99";
    vector<byte> data(text.size());
    crypto::chacha20 cipher(key, nonce);
    cipher.seek(1);
    cipher.xor_key_stream(data.as_slice(0, 10), text.as_slice(0, 10));
    cipher.xor_key_stream(data.as_slice(10), text.as_slice(10));
    println("{}", encoding::hex::encode(data));

    // The last block of the keystream, and past it
    cipher.seek(0xffffffff);
    vector<byte> block(64);
    cipher.xor_key_stream(block, block);
    try {
        cipher.xor_key_stream(block.as_slice(0, 1), block.as_slice(0, 1));
    } catch (const length_error& e) {
        println("{}", e.what());
    }
}
```

Output:

```text
6e2e359a2568f98041ba0728dd0d6981e97e7aec1d4360c20a27afccfd9fae0bf91b65c5524733ab
sgcl::crypto::chacha20::xor_key_stream: past the end of the keystream (2^32 blocks)
```

## See also

- [seek](seek.md): moves to the start of a block
- [sgcl::crypto::chacha20](../chacha20.md)
