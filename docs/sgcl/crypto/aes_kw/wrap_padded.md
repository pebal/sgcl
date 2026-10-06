[sgcl](../../README.md) › [crypto](../README.md) › [aes_kw](README.md)

# sgcl::crypto::aes_kw::wrap_padded

```cpp
vector<byte> wrap_padded(const slice<const byte>& key) const;
```

Wraps `key` by RFC 5649's algorithm: a key of any length, padded with zeros to a multiple of 8, its length kept in
the register beside the constant `A65959A6`, so that the padding comes off on unwrapping. A key of 8 bytes or fewer is
one AES encryption of the register and the key; a longer one goes through RFC 3394's six passes.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key to wrap, 1 to 2^32 - 1 bytes |

## Return value

The wrapped key, the key's length rounded up to a multiple of 8, plus 8 bytes; not a secret.

## Complexity

Linear in `key.size()`.

## Exceptions

`invalid_argument` when `key` is empty or 2^32 bytes or more; `logic_error` when the object was moved from.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // RFC 5649 §6: 20 bytes and 7 bytes of key under a 192-bit key-encryption key
    crypto::aes_kw wrapper(encoding::hex::decode("5840df6e29b02af1ab493b705bf16ea1ae8338f4dcc176a8"));
    println(encoding::hex::encode(wrapper.wrap_padded(encoding::hex::decode("c37b7e6492584340bed12207808941155068f738"))));
    println(encoding::hex::encode(wrapper.wrap_padded(encoding::hex::decode("466f7250617369"))));
}
```

Output:

```text
138bdeaa9b8fa7fc61f97742e72248ee5ae6ae5360d1ae6a5f54f373fa543b6a
afbeb0f07dfbf5419200f2ccb50bb24f
```

## See also

- [unwrap_padded](unwrap_padded.md): the key back
- [wrap](wrap.md): RFC 3394's, for keys of 8 n bytes
- [sgcl::crypto::aes_kw](README.md)
