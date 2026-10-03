[sgcl](../../README.md) › [crypto](../README.md) › [mlkem768](../mlkem.md) › [encapsulation_key](../mlkem768-encapsulation_key.md)

# sgcl::crypto::mlkem768::encapsulation_key::bytes

```cpp
vector<byte> bytes() const noexcept;
```

The key's bytes (FIPS 203's ek), the form it is published in and [from_bytes](from_bytes.md) reads, Go's `Bytes`:
`encapsulation_key_size` bytes, 1184 for ML-KEM-768, 800 for `mlkem512` and 1568 for `mlkem1024`.

## Parameters

None.

## Return value

A new vector of `encapsulation_key_size` bytes.

## Complexity

Linear in the size of the key.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto owner = crypto::mlkem768::decapsulation_key::from_seed(encoding::hex::decode(
        "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"
        "202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f"));
    vector<byte> published = owner->encapsulation_key().bytes();

    println("{} bytes", published.size());
    println("SHA-256 {}", encoding::hex::encode(crypto::sha256::of(published)));
    println("{}", crypto::mlkem768::encapsulation_key::from_bytes(published)->bytes() == published);
}
```

Output:

```text
1184 bytes
SHA-256 0b7934c83125c788995e2ba6bd761e33046b3e40571be53e023309a29f398cc9
true
```

## See also

- [from_bytes](from_bytes.md): the key of its bytes
- [sgcl::crypto::mlkem768::encapsulation_key](../mlkem768-encapsulation_key.md)
