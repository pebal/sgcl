[sgcl](../../README.md) › [crypto](../README.md) › [aes_ctr](../aes_ctr.md)

# sgcl::crypto::aes_ctr::clone

```cpp
aes_ctr clone() const;
```

A second object with the same key, the same initial counter and the same place in the keystream, the unused bytes
of a block begun with it: a copy of a secret, made on purpose and by name. From there the two go on each on its
own.

## Parameters

None.

## Return value

An object with the state of this one.

## Complexity

Constant.

## Exceptions

`logic_error` when the object was moved from and holds no key.

## Notes

The two give the same keystream from the same place: a clone is for decrypting what the original encrypted, or for
trying a part twice, never for encrypting two messages.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector<byte> key = encoding::hex::decode("2b7e151628aed2a6abf7158809cf4f3c");
    vector<byte> iv = encoding::hex::decode("f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff");
    crypto::aes_ctr ctr(key, iv);
    vector<byte> skipped(10);
    ctr.xor_key_stream(skipped, skipped);

    crypto::aes_ctr copy = ctr.clone();  // in the middle of the first block
    vector<byte> a(6), b(6);
    ctr.xor_key_stream(a, a);
    copy.xor_key_stream(b, b);
    println("{} {}", encoding::hex::encode(a), a == b);
}
```

Output:

```text
1675ea9ea1e4 true
```

## See also

- [(constructor)](aes_ctr.md): sets up a key and a counter, or takes another object's over
- [seek](seek.md): moves to the start of a block
- [sgcl::crypto::aes_ctr](../aes_ctr.md)
