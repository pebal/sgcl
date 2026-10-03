[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [name](../x509-name.md)

# sgcl::crypto::x509::name::empty

```cpp
bool empty() const noexcept;
```

Checks whether the name has no attribute. A certificate whose subject is empty is identified by its subject
alternative names alone.

## Parameters

None.

## Return value

`true` when the name has no attribute, `false` otherwise.

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
    auto text = io::read_text("tests/net/tls_testdata/rsa.pem");  // a leaf of the tree's test CA
    crypto::x509::certificate cert = crypto::x509::certificate::from_pem(text);

    println("{} {}", cert.subject().empty(), crypto::x509::name().empty());
}
```

Output:

```text
false true
```

## See also

- [attributes](attributes.md)
- [sgcl::crypto::x509::name](../x509-name.md)
