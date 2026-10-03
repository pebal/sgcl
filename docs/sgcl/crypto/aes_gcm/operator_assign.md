[sgcl](../../README.md) › [crypto](../README.md) › [aes_gcm](../aes_gcm.md)

# sgcl::crypto::aes_gcm::operator=

```cpp
/*(1)*/ aes_gcm& operator=(aes_gcm&& other) noexcept;
/*(2)*/ aes_gcm& operator=(const aes_gcm&) = delete;
```

1. Takes the key of `other` over, written over the key this object held; `other` is overwritten with zeros and
   holds no key. An assignment of an object to itself does nothing.
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
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // The GCM specification's test case 13: a zero key of 256 bits, nothing to seal
    crypto::aes_gcm gcm(vector<byte>(16));
    crypto::aes_gcm next(vector<byte>(32));
    gcm = std::move(next);  // the key of 128 bits is overwritten
    vector<byte> nothing;
    println("{} {}", gcm.key_size(), encoding::hex::encode(gcm.seal(vector<byte>(12), nothing)));
    println("{}", next.key_size());
}
```

Output:

```text
32 530f8afbc74536b9a963b4f1c4cb738b
0
```

## See also

- [(constructor)](aes_gcm.md): sets up a key, or takes another object's over
- [clone](clone.md): a copy of the key, made on purpose
- [sgcl::crypto::aes_gcm](../aes_gcm.md)
