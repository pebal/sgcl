[sgcl](../../README.md) › [crypto](../README.md) › [aes](README.md)

# sgcl::crypto::aes::operator=

```cpp
aes& operator=(aes&& other) noexcept;    // (1)
aes& operator=(const aes&) = delete;     // (2)
```

1. Takes the key schedule of `other` over, written over the one this object held; `other` is overwritten with
   zeros and holds no key. An assignment of an object to itself does nothing.
2. The key schedule is not copied by accident: a copy is [clone](clone.md).

## Parameters

| Parameter | Description |
|---|---|
| `other` | the object whose key schedule is taken over |

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
    crypto::aes cipher(vector<byte>(16));
    crypto::aes next(vector<byte>(32));
    cipher = std::move(next);  // the key of 128 bits is overwritten
    println("{} {}", cipher.key_size(), next.key_size());
    try {
        next.encrypt_block(array<byte, 16>());
    } catch (const logic_error& e) {
        println("{}", e.what());
    }
}
```

Output:

```text
32 0
sgcl::crypto::aes: used after being moved from
```

## See also

- [(constructor)](aes.md): sets up a key schedule, or takes another object's over
- [clone](clone.md): a copy of the key schedule, made on purpose
- [sgcl::crypto::aes](README.md)
