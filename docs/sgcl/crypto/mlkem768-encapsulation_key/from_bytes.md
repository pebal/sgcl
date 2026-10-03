[sgcl](../../README.md) › [crypto](../README.md) › [mlkem768](../mlkem.md) › [encapsulation_key](../mlkem768-encapsulation_key.md)

# sgcl::crypto::mlkem768::encapsulation_key::from_bytes

```cpp
static expected<encapsulation_key, error> from_bytes(const slice<const byte>& bytes) noexcept;
```

Reads an encapsulation key from the bytes its owner published (FIPS 203's ek), Go's `NewEncapsulationKey768`. The
key is checked as FIPS 203 §7.2 asks: its length must be `encapsulation_key_size` and every coefficient of its
vector, twelve bits each, below q = 3329. The matrix Â that the key's encapsulations need is made here, once, from
the 32 bytes of ρ at the key's end.

`mlkem512::encapsulation_key::from_bytes` takes 800 bytes and `mlkem1024::encapsulation_key::from_bytes` 1568.

## Parameters

| Parameter | Description |
|---|---|
| `bytes` | the key, `encapsulation_key_size` bytes (1184) |

## Return value

The key, or a [crypto::error](../error.md) with `errc::invalid_key` for a key of another length or with a
coefficient not below q.

## Complexity

Linear in the size of the key, and the sampling of the matrix: k² polynomials from SHAKE128 (9 for ML-KEM-768),
whose rejection sampling takes the time the key's bytes ask for.

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

    auto key = crypto::mlkem768::encapsulation_key::from_bytes(published);
    println("{}", key.has_value());

    auto cut = crypto::mlkem768::encapsulation_key::from_bytes(published.as_slice(0, 1000));
    println("{}", cut.error().message());

    vector<byte> broken = published;
    broken[0] = broken[1] = byte(0xff);  // the first coefficient 4095, not below q
    auto refused = crypto::mlkem768::encapsulation_key::from_bytes(broken);
    println("{}", refused.error().code() == crypto::errc::invalid_key);
}
```

Output:

```text
true
sgcl::crypto::mlkem768: an encapsulation key of the wrong length or with a coefficient not below q
true
```

## See also

- [bytes](bytes.md): the bytes the key is read from
- [mlkem768::decapsulation_key::encapsulation_key](../mlkem768-decapsulation_key/encapsulation_key.md): the key of a
  decapsulation key, without its bytes read again
- [sgcl::crypto::mlkem768::encapsulation_key](../mlkem768-encapsulation_key.md)
