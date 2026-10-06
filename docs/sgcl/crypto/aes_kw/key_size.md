[sgcl](../../README.md) › [crypto](../README.md) › [aes_kw](README.md)

# sgcl::crypto::aes_kw::key_size

```cpp
size_t key_size() const noexcept;
```

The length of the key-encryption key in bytes, 16, 24 or 32; 0 after the object was moved from.

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
    crypto::aes_kw wrapper(vector<byte>(24));
    crypto::aes_kw taken = std::move(wrapper);
    println("{} {}", wrapper.key_size(), taken.key_size());
}
```

Output:

```text
0 24
```

## See also

- [(constructor)](aes_kw.md): the key
- [sgcl::crypto::aes_kw](README.md)
