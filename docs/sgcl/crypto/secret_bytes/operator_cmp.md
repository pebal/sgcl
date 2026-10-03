[sgcl](../../README.md) › [crypto](../README.md) › [secret_bytes](README.md)

# sgcl::crypto::operator== (sgcl::crypto::secret_bytes)

```cpp
friend bool operator==(const secret_bytes& a, const secret_bytes& b) noexcept;
```

Compares the bytes of two secrets in constant time, as [constant_time::equal](../constant_time/equal.md) does:
every byte of both is read whatever they hold. The lengths are not secret: two secrets of different lengths are
unequal at once. `!=` is made from it by the compiler.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the secrets compared |

## Return value

Whether the secrets hold the same bytes.

## Complexity

Linear in the length when the lengths are equal; constant otherwise.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // RFC 8032's TEST 1 seed: the same key exported twice
    auto key = crypto::ed25519::private_key::from_seed(
        encoding::hex::decode("9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60"));
    println("{}", key->to_pkcs8_der() == key->to_pkcs8_der());
    println("{}", key->to_pkcs8_der() == key->to_pem());
}
```

Output:

```text
true
false
```

## See also

- [constant_time::equal](../constant_time/equal.md): the comparison of any bytes
- [sgcl::crypto::secret_bytes](README.md)
