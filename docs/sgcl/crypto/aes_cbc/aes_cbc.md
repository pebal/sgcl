[sgcl](../../README.md) › [crypto](../README.md) › [aes_cbc](README.md)

# sgcl::crypto::aes_cbc::aes_cbc

```cpp
aes_cbc(const slice<const byte>& key, const slice<const byte>& iv);    // (1)
aes_cbc(aes_cbc&& other) noexcept;                                     // (2)
aes_cbc(const aes_cbc&) = delete;                                      // (3)
```

1. Sets up the key schedules of `key` for both directions, and the chain at `iv`.
2. Takes the state of `other` over; `other` is overwritten with zeros and holds no key.
3. The state is not copied by accident: a copy is [clone](clone.md).

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key, 16, 24 or 32 bytes: AES-128, AES-192, AES-256 |
| `iv` | the IV, 16 bytes, unpredictable for each message |
| `other` | the object whose state is taken over |

## Complexity

Constant.

## Exceptions

- (1) `invalid_argument` when `key` is not 16, 24 or 32 bytes, or `iv` is not 16.
- (2) None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector<byte> key(32), iv(16);
    crypto::aes_cbc cbc(key, iv);
    println("{}", cbc.key_size());
    try {
        crypto::aes_cbc wrong(key, vector<byte>(12));
    } catch (const invalid_argument& e) {
        println("{}", e.what());
    }
}
```

Output:

```text
32
sgcl::crypto::aes_cbc: an IV of 12 bytes, not 16
```

## See also

- [from_key](from_key.md): a key that came with data
- [reset](reset.md): a new IV under the same key
- [sgcl::crypto::aes_cbc](README.md)
