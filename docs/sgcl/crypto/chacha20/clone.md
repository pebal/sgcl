[sgcl](../../README.md) › [crypto](../README.md) › [chacha20](../chacha20.md)

# sgcl::crypto::chacha20::clone

```cpp
chacha20 clone() const;
```

A second object with the same key, the same nonce and the same place in the keystream, the unused bytes of a block
begun with it: a copy of a secret, made on purpose and by name. From there the two go on each on its own.

## Parameters

None.

## Return value

An object with the state of this one.

## Complexity

Constant.

## Exceptions

`logic_error` when the object was moved from and holds no key.

## Notes

The two give the same keystream from the same place: a clone is for decrypting what the original encrypted, never
for encrypting two messages.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // RFC 8439 §2.3.2: the first bytes of block 1 from either copy
    vector<byte> key =
        encoding::hex::decode("000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f");
    vector<byte> nonce = encoding::hex::decode("000000090000004a00000000");
    crypto::chacha20 cipher(key, nonce);
    cipher.seek(1);
    crypto::chacha20 copy = cipher.clone();
    vector<byte> a(8), b(8);
    cipher.xor_key_stream(a, a);
    copy.xor_key_stream(b, b);
    println("{} {}", encoding::hex::encode(a), a == b);
}
```

Output:

```text
10f1e7e4d13b5915 true
```

## See also

- [(constructor)](chacha20.md): sets up a key and a nonce, or takes another object's over
- [seek](seek.md): moves to the start of a block
- [sgcl::crypto::chacha20](../chacha20.md)
