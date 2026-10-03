[sgcl](../../README.md) › [crypto](../README.md) › [secret_bytes](../secret_bytes.md)

# sgcl::crypto::secret_bytes::size

```cpp
size_t size() const noexcept;
```

Returns the number of bytes. The length of a secret is not secret: it is fixed by the protocol or the algorithm.

## Parameters

None.

## Return value

The number of bytes.

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
    // RFC 8032's TEST 1 seed: its PKCS #8 and its PEM
    auto key = crypto::ed25519::private_key::from_seed(
        encoding::hex::decode("9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60"));
    println("{} {}", key->to_pkcs8_der().size(), key->to_pem().size());
}
```

Output:

```text
48 119
```

## See also

- [empty](empty.md): whether there are no bytes
- [resize](resize.md): changes the number of bytes
- [sgcl::crypto::secret_bytes](../secret_bytes.md)
