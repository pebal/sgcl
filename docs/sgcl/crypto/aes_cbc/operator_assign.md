[sgcl](../../README.md) › [crypto](../README.md) › [aes_cbc](README.md)

# sgcl::crypto::aes_cbc::operator=

```cpp
aes_cbc& operator=(aes_cbc&& other) noexcept;    // (1)
aes_cbc& operator=(const aes_cbc&) = delete;     // (2)
```

1. Takes the state of `other` over, its chain with it, written over the state this object held; `other` is
   overwritten with zeros and holds no key. An assignment of an object to itself does nothing.
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
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    crypto::aes_cbc cbc(vector<byte>(16), vector<byte>(16));
    crypto::aes_cbc other(vector<byte>(32), vector<byte>(16));
    cbc = std::move(other);
    println("{} {}", cbc.key_size(), other.key_size());
}
```

Output:

```text
32 0
```

## See also

- [clone](clone.md): a copy made on purpose
- [sgcl::crypto::aes_cbc](README.md)
