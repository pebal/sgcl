[sgcl](../../README.md) › [crypto](../README.md) › [aes_ctr](../aes_ctr.md)

# sgcl::crypto::aes_ctr::operator=

```cpp
/*(1)*/ aes_ctr& operator=(aes_ctr&& other) noexcept;
/*(2)*/ aes_ctr& operator=(const aes_ctr&) = delete;
```

1. Takes the state of `other` over, its place in the keystream with it, written over the state this object held;
   `other` is overwritten with zeros and holds no key. An assignment of an object to itself does nothing.
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
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // SP 800-38A F.5.1: the second block, after the first went through another variable
    vector<byte> key = encoding::hex::decode("2b7e151628aed2a6abf7158809cf4f3c");
    vector<byte> iv = encoding::hex::decode("f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff");
    vector<byte> text = encoding::hex::decode(
        "6bc1bee22e409f96e93d7e117393172aae2d8a571e03ac9c9eb76fac45af8e51");
    crypto::aes_ctr first(key, iv);
    first.xor_key_stream(text.as_slice(0, 16), text.as_slice(0, 16));

    crypto::aes_ctr ctr(vector<byte>(16), vector<byte>(16));
    ctr = std::move(first);
    ctr.xor_key_stream(text.as_slice(16), text.as_slice(16));
    println("{}", encoding::hex::encode(text.as_slice(16)));
    println("{}", first.key_size());
}
```

Output:

```text
9806f66b7970fdff8617187bb9fffdff
0
```

## See also

- [(constructor)](aes_ctr.md): sets up a key and a counter, or takes another object's over
- [clone](clone.md): a copy that goes on from the same place
- [sgcl::crypto::aes_ctr](../aes_ctr.md)
