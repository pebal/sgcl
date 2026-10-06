[sgcl](../../README.md) › [crypto](../README.md) › [aes_kw](README.md)

# sgcl::crypto::aes_kw::aes_kw

```cpp
explicit aes_kw(const slice<const byte>& kek);    // (1)
aes_kw(aes_kw&& other) noexcept;                  // (2)
aes_kw(const aes_kw&) = delete;                   // (3)
```

1. Sets up the key schedules of the key-encryption key `kek`, for wrapping and for unwrapping.
2. Takes the key of `other` over; `other` is overwritten with zeros and holds no key.
3. The key is not copied by accident: a copy is [clone](clone.md).

## Parameters

| Parameter | Description |
|---|---|
| `kek` | the key-encryption key, 16, 24 or 32 bytes |
| `other` | the object whose key is taken over |

## Complexity

Constant.

## Exceptions

- (1) `invalid_argument` when `kek` is not 16, 24 or 32 bytes.
- (2) None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    crypto::aes_kw wrapper(vector<byte>(32));
    println("{}", wrapper.key_size());
}
```

Output:

```text
32
```

## See also

- [from_key](from_key.md): a key-encryption key that came with data
- [sgcl::crypto::aes_kw](README.md)
