[sgcl](../../README.md) › [crypto](../README.md) › [chacha20](../chacha20.md)

# sgcl::crypto::chacha20::chacha20

```cpp
chacha20(const slice<const byte>& key, const slice<const byte>& nonce);    // (1)
chacha20(chacha20&& other) noexcept;                                       // (2)
chacha20(const chacha20&) = delete;                                        // (3)
```

1. Sets up the key and the nonce, the keystream at block 0. A nonce of 12 bytes is RFC 8439's cipher; one of 24
   bytes is XChaCha20, the key and the nonce's first 16 bytes through HChaCha20 to a new key and the last 8 bytes
   the nonce.
2. Takes the state of `other` over, its place in the keystream with it; `other` is overwritten with zeros and holds
   no key, and any call on it but the assignment and the destructor throws `logic_error`.
3. The state is a secret and is not copied by accident: a copy is asked for by name, [clone](clone.md).

## Parameters

| Parameter | Description |
|---|---|
| `key` | 32 bytes |
| `nonce` | 12 bytes, or 24 for XChaCha20 |
| `other` | the object whose state is taken over |

## Complexity

Constant.

## Exceptions

- (1) `invalid_argument` when `key` is not 32 bytes, or `nonce` not 12 or 24.
- (2) None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // RFC 8439 §2.3.2: the block of counter 1
    vector<byte> key =
        encoding::hex::decode("000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f");
    vector<byte> nonce = encoding::hex::decode("000000090000004a00000000");
    crypto::chacha20 cipher(key, nonce);
    cipher.seek(1);
    vector<byte> block(64);
    cipher.xor_key_stream(block, block);
    println("{}", encoding::hex::encode(block.as_slice(0, 32)));
    println("{}", encoding::hex::encode(block.as_slice(32)));

    // XChaCha20, the nonce of the draft's §A.3.1: the first 20 bytes of its ciphertext
    vector<byte> x_key =
        encoding::hex::decode("808182838485868788898a8b8c8d8e8f909192939495969798999a9b9c9d9e9f");
    vector<byte> x_nonce =
        encoding::hex::decode("404142434445464748494a4b4c4d4e4f5051525354555657");
    crypto::chacha20 x(x_key, x_nonce);
    x.seek(1);
    string text = "Ladies and Gentlemen";
    vector<byte> out(text.size());
    x.xor_key_stream(out, text);
    println("{}", encoding::hex::encode(out));

    try {
        crypto::chacha20 wrong(key, nonce.as_slice(0, 8));
    } catch (const invalid_argument& e) {
        println("{}", e.what());
    }
}
```

Output:

```text
10f1e7e4d13b5915500fdd1fa32071c4c7d1f4c733c068030422aa9ac3d46c4e
d2826446079faa0914c2d705d98b02a2b5129cd1de164eb9cbd083e8a2503c4e
bd6d179d3e83d43b9576579493c0e939572a1700
sgcl::crypto::chacha20: a nonce of 8 bytes, not 12 or 24
```

## See also

- [from_key](from_key.md): a key that came with data
- [clone](clone.md): a copy that goes on from the same place
- [operator=](operator_assign.md): takes another object's state over
- [sgcl::crypto::chacha20](../chacha20.md)
