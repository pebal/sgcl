[sgcl](../../README.md) › [crypto](../README.md) › [aes_gcm](README.md)

# sgcl::crypto::aes_gcm::key_size

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
    crypto::aes_gcm gcm(vector<byte>(24));
    println("{}", gcm.key_size());
    crypto::aes_gcm taken = std::move(gcm);
    println("{} {}", gcm.key_size(), taken.key_size());
}
```

Output:

```text
24
0 24
```

## See also

- [(constructor)](aes_gcm.md): sets up a key of 16, 24 or 32 bytes
- [sgcl::crypto::aes_gcm](README.md)
