[sgcl](../../README.md) › [crypto](../README.md) › [aes_kw](README.md)

# sgcl::crypto::aes_kw::operator=

```cpp
aes_kw& operator=(aes_kw&& other) noexcept;    // (1)
aes_kw& operator=(const aes_kw&) = delete;     // (2)
```

1. Takes the key of `other` over, written over the key this object held; `other` is overwritten with zeros and holds
   no key. An assignment of an object to itself does nothing.
2. The key is not copied by accident: a copy is [clone](clone.md).

## Parameters

| Parameter | Description |
|---|---|
| `other` | the object whose key is taken over |

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
    crypto::aes_kw wrapper(vector<byte>(16));
    wrapper = crypto::aes_kw(vector<byte>(32));
    println("{}", wrapper.key_size());
}
```

Output:

```text
32
```

## See also

- [clone](clone.md): a copy made on purpose
- [sgcl::crypto::aes_kw](README.md)
