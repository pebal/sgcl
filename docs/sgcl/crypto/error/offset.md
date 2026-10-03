[sgcl](../../README.md) › [crypto](../README.md) › [error](../error.md)

# sgcl::crypto::error::offset

```cpp
uint64_t offset() const noexcept;
```

Returns the byte of the encoded input where the error was found, counted from the start of the input: where the
reading of DER stopped, or where a key of another algorithm names its algorithm. For data that is not an encoding
(a tag, a key's bytes) it is 0, and so it is for an error found at the first byte.

## Parameters

None.

## Return value

The offset in bytes, or 0.

## Complexity

Constant.

## Exceptions

None.

## Notes

A non-zero offset is the start of [message](message.md): `offset 9: …`.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // a SubjectPublicKeyInfo of Ed25519 whose key is 33 bytes long
    auto der = encoding::hex::decode(
        "302a300506032b6570032200d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a");
    auto key = crypto::ed25519::public_key::from_pkix_der(der);
    println("{}", key.error().offset());
    println("{}", key.error().message());

    auto short_key = crypto::ed25519::public_key::from_bytes(encoding::hex::decode("d75a98"));
    println("{}", short_key.error().offset());
}
```

Output:

```text
9
offset 9: the public key is not a BIT STRING of 32 bytes
0
```

## See also

- [message](message.md): the error as a text, with the offset
- [sgcl::crypto::error](../error.md)
