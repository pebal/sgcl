[sgcl](../../README.md) › [crypto](../README.md) › [chacha20](README.md)

# sgcl::crypto::chacha20::operator=

```cpp
chacha20& operator=(chacha20&& other) noexcept;    // (1)
chacha20& operator=(const chacha20&) = delete;     // (2)
```

1. Takes the state of `other` over, its place in the keystream with it, written over the state this object held;
   `other` is overwritten with zeros and holds no key. An assignment of an object to itself does nothing.
2. The state is not copied by accident: a copy is [clone](clone.md).

## Parameters

| Parameter | Description |
|---|---|
| `other` | the object whose state is taken over |

## Return value

`*this`.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // RFC 8439 §2.3.2: block 1, after block 0 went through another variable
    vector<byte> key =
        encoding::hex::decode("000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f");
    vector<byte> nonce = encoding::hex::decode("000000090000004a00000000");
    crypto::chacha20 first(key, nonce);
    vector<byte> block0(64);
    first.xor_key_stream(block0, block0);

    crypto::chacha20 cipher(vector<byte>(32), vector<byte>(12));
    cipher = std::move(first);
    vector<byte> block1(16);
    cipher.xor_key_stream(block1, block1);
    println("{}", encoding::hex::encode(block1));
    try {
        first.seek(0);
    } catch (const logic_error& e) {
        println("{}", e.what());
    }
}
```

Output:

```text
10f1e7e4d13b5915500fdd1fa32071c4
sgcl::crypto::chacha20: used after being moved from
```

## See also

- [(constructor)](chacha20.md): sets up a key and a nonce, or takes another object's over
- [clone](clone.md): a copy that goes on from the same place
- [sgcl::crypto::chacha20](README.md)
