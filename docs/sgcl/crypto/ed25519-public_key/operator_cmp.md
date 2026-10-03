[sgcl](../../README.md) › [crypto](../README.md) › [ed25519](../ed25519.md) › [public_key](README.md)

# sgcl::crypto::ed25519::operator== (sgcl::crypto::ed25519::public_key)

```cpp
friend bool operator==(const public_key& a, const public_key& b) noexcept;
```

Compares the bytes of two keys. A public key is not a secret, so the comparison is an ordinary one, which may stop at
the first byte that differs; and since a key is only ever a canonical encoding, equal points are equal bytes. `!=` is
made from it by the compiler.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the keys compared |

## Return value

Whether the keys are the same 32 bytes.

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
    // RFC 8032's TEST 1 key: computed from the seed, and read from the SubjectPublicKeyInfo
    auto key = crypto::ed25519::private_key::from_seed(
        encoding::hex::decode("9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60"));
    auto read = crypto::ed25519::public_key::from_pkix_der(key->public_key().to_pkix_der());
    println("{}", read.value() == key->public_key());
}
```

Output:

```text
true
```

## See also

- [bytes](bytes.md): the bytes compared
- [sgcl::crypto::ed25519::public_key](README.md)
