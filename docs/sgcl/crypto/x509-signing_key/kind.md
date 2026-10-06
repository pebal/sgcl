[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [signing_key](README.md)

# sgcl::crypto::x509::signing_key::kind

```cpp
key_kind kind() const noexcept;
```

The kind of the key the view is of: `key_kind::p256`, `p384`, `p521`, `ed25519` or `rsa` ([key_kind](../x509-key_kind.md)).

## Parameters

None.

## Return value

The kind.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto key = crypto::rsa::private_key::generate(2048);
    crypto::x509::signing_key signer = key;
    println("{}", signer.kind() == crypto::x509::key_kind::rsa);
}
```

Output:

```text
true
```

## See also

- [key_kind](../x509-key_kind.md)
- [sgcl::crypto::x509::signing_key](README.md)
