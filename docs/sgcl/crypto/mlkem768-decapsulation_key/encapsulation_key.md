[sgcl](../../README.md) › [crypto](../README.md) › [mlkem768](../mlkem.md) › [decapsulation_key](../mlkem768-decapsulation_key.md)

# sgcl::crypto::mlkem768::decapsulation_key::encapsulation_key

```cpp
mlkem768::encapsulation_key encapsulation_key() const;
```

The public key of this key, Go's `EncapsulationKey`: the [encapsulation_key](../mlkem768-encapsulation_key.md) the
owner publishes, whose bytes anyone reads and encapsulates to. It is taken from the expanded key with the matrix the
key already has, so nothing is sampled again. `mlkem512::decapsulation_key::encapsulation_key` gives a
`mlkem512::encapsulation_key`, `mlkem1024`'s a `mlkem1024::encapsulation_key`.

## Parameters

None.

## Return value

The encapsulation key.

## Complexity

Linear in the size of the encapsulation key: its bytes and its matrix are copied and its bytes hashed.

## Exceptions

`std::logic_error` when the key was moved from.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto key = crypto::mlkem768::decapsulation_key::from_seed(encoding::hex::decode(
        "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"
        "202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f"));
    auto published = key->encapsulation_key().bytes();
    println("{}...", encoding::hex::encode(published).substr(0, 32));
    println("{}", crypto::mlkem768::encapsulation_key::from_bytes(published)
                      == key->encapsulation_key());
}
```

Output:

```text
298aa10d423c8dda069d02bc59e6cdf0...
true
```

## See also

- [mlkem768::encapsulation_key](../mlkem768-encapsulation_key.md): what it gives
- [sgcl::crypto::mlkem768::decapsulation_key](../mlkem768-decapsulation_key.md)
