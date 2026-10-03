[sgcl](../../README.md) › [crypto](../README.md) › [mlkem768](../mlkem.md) › [encapsulation_key](../mlkem768-encapsulation_key.md)

# sgcl::crypto::mlkem768::operator== (sgcl::crypto::mlkem768::encapsulation_key)

```cpp
friend bool operator==(const encapsulation_key& a, const encapsulation_key& b) noexcept;
```

Compares the bytes of two keys, Go's `Equal`: two keys are equal when their bytes are, and then their matrices are
too. `!=` is its negation, written by the language. The key is public, so the comparison is not in constant time.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the keys to compare |

## Return value

`true` when the keys have the same bytes, `false` otherwise.

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
    auto key = owner->encapsulation_key();
    auto read = crypto::mlkem768::encapsulation_key::from_bytes(key.bytes());

    println("{}", key == read);
    println("{}", key != crypto::mlkem768::decapsulation_key::generate().encapsulation_key());
}
```

Output:

```text
true
true
```

## See also

- [mlkem768::decapsulation_key: operator==](../mlkem768-decapsulation_key/operator_cmp.md): the secret keys,
  compared in constant time
- [sgcl::crypto::mlkem768::encapsulation_key](../mlkem768-encapsulation_key.md)
