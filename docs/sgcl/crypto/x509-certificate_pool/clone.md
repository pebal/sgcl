[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [certificate_pool](../x509-certificate_pool.md)

# sgcl::crypto::x509::certificate_pool::clone

```cpp
certificate_pool clone() const noexcept;
```

Makes a pool of its own with the same certificates: an addition to either is not seen by the other. A copy of a pool
shares; a clone does not.

## Parameters

None.

## Return value

The new pool.

## Complexity

Linear in the number of certificates.

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

    auto roots = crypto::x509::certificate_pool::from_file("tests/net/tls_testdata/ca.pem");
    auto mine = roots->clone();
    mine.add(cert);
    println("{} {}", roots->size(), mine.size());
}
```

Output:

```text
1 2
```

## See also

- [add](add.md)
- [sgcl::crypto::x509::certificate_pool](../x509-certificate_pool.md)
