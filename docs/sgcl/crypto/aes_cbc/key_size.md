[sgcl](../../README.md) › [crypto](../README.md) › [aes_cbc](README.md)

# sgcl::crypto::aes_cbc::key_size

```cpp
size_t key_size() const noexcept;
```

The length of the key in bytes, 16, 24 or 32 (AES-128, AES-192, AES-256); 0 after the object was moved from.

## Parameters

None.

## Return value

16, 24 or 32, or 0.

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
    crypto::aes_cbc cbc(vector<byte>(24), vector<byte>(16));
    println("{}", cbc.key_size());
    crypto::aes_cbc taken = std::move(cbc);
    println("{} {}", cbc.key_size(), taken.key_size());
}
```

Output:

```text
24
0 24
```

## See also

- [(constructor)](aes_cbc.md): the key
- [sgcl::crypto::aes_cbc](README.md)
