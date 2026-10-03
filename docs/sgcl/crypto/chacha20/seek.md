[sgcl](../../README.md) › [crypto](../README.md) › [chacha20](../chacha20.md)

# sgcl::crypto::chacha20::seek

```cpp
void seek(uint32_t counter);
```

Moves to the start of block `counter` of the keystream, forwards or backwards; the part of a block left from the
last call is dropped and zeroed. Go's `SetCounter`, without its refusal to go back, which is there to stop a
keystream from being reused by mistake: here a seek back is for decrypting from the middle, and reusing a keystream
to encrypt is the program's mistake to avoid.

## Parameters

| Parameter | Description |
|---|---|
| `counter` | the number of the block, 0 for the start |

## Return value

None.

## Complexity

Constant.

## Exceptions

`logic_error` when the object was moved from and holds no key.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // RFC 8439 §2.6.2: the Poly1305 key is the first 32 bytes of block 0
    vector<byte> key =
        encoding::hex::decode("808182838485868788898a8b8c8d8e8f909192939495969798999a9b9c9d9e9f");
    vector<byte> nonce = encoding::hex::decode("000000000001020304050607");
    crypto::chacha20 cipher(key, nonce);
    vector<byte> one_time_key(32);
    cipher.xor_key_stream(one_time_key, one_time_key);
    println("{}", encoding::hex::encode(one_time_key));

    cipher.seek(0);  // back to the start, and the same bytes again
    vector<byte> again(32);
    cipher.xor_key_stream(again, again);
    println("{}", again == one_time_key);
}
```

Output:

```text
8ad5a08b905f81cc815040274ab29471a833b637e3fd0da508dbb8e2fdd1a646
true
```

## See also

- [xor_key_stream](xor_key_stream.md): XORs the keystream into the data
- [sgcl::crypto::chacha20](../chacha20.md)
