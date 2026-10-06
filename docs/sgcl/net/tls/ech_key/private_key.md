[sgcl](../../../README.md) › [net](../../README.md) › [tls](../README.md) › [ech_key](README.md)

# sgcl::net::tls::ech_key::private_key

```cpp
crypto::secret_bytes private_key() const;
```

The HPKE private key (SerializePrivateKey of [crypto::hpke](../../../crypto/hpke.md)), in plain memory: what [from_bytes](from_bytes.md) takes back.

## Parameters

None.

## Return value

The key, in a secret_bytes.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"
#include "sgcl/net/tls.h"

using namespace sgcl;

int main() {
    auto key = net::tls::ech_key::generate("public.example");
    println("{}", key.private_key().size());
}
```

Output:

```text
32
```

## See also

- [from_bytes](from_bytes.md)
- [sgcl::net::tls::ech_key](README.md)
