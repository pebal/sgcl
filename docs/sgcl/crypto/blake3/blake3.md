[sgcl](../../README.md) › [crypto](../README.md) › [blake3](README.md)

# sgcl::crypto::blake3::blake3

```cpp
blake3() noexcept;                                // (1)
explicit blake3(const slice<const byte>& key);    // (2)
blake3(const blake3& other) noexcept;             // (3)
```

1. Makes a hasher in the hash mode that has hashed nothing: its `value()` is the hash of the empty input.
2. Makes a hasher in the keyed_hash mode, a MAC under `key`, which must be 32 bytes: the key is the starting chaining
   value of every chunk and parent, and the hasher keeps its words until it is destroyed, when it zeroes them.
3. A copy: the hasher goes on from where `other` stood, in its mode; each zeroes its own state when keyed.

The derive_key mode is made by [for_derive_key](for_derive_key.md).

## Parameters

| Parameter | Description |
|---|---|
| `key` | the secret key, 32 bytes |
| `other` | the hasher to copy |

## Complexity

Constant.

## Exceptions

- (1), (3) None.
- (2) `std::invalid_argument` when `key` is not 32 bytes long.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    println(encoding::hex::encode(crypto::blake3().value()));

    vector<byte> key(32);
    for (int i : range(32)) {
        key[i] = byte(i);
    }
    crypto::blake3 mac(key);
    mac.update("message");
    println(encoding::hex::encode(mac.value()));
}
```

Output:

```text
af1349b9f5f9a1a6a0404dea36dcc9499bcb25c9adc112b7cc9a93cae41f3262
0978071f9c601ec34611813742454dd142c63ffb2cacac65a20b38253323bb00
```

## See also

- [for_derive_key](for_derive_key.md): the third mode
- [reset](reset.md): back to what the constructor made
- [sgcl::crypto::blake3](README.md)
