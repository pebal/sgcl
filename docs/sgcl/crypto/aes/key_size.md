[sgcl](../../README.md) › [crypto](../README.md) › [aes](README.md)

# sgcl::crypto::aes::key_size

```cpp
size_t key_size() const noexcept;
```

The length of the key in bytes: 16 for AES-128, 24 for AES-192, 32 for AES-256, and 0 for an object moved from,
which holds no key.

## Parameters

None.

## Return value

16, 24, 32 or 0.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    crypto::aes cipher(vector<byte>(32));
    println("{}", cipher.key_size());
    crypto::aes taken = std::move(cipher);
    println("{} {}", cipher.key_size(), taken.key_size());
}
```

Output:

```text
32
0 32
```

## See also

- [(constructor)](aes.md): sets up a key of 16, 24 or 32 bytes
- [sgcl::crypto::aes](README.md)
