[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [certificate_pool](../x509-certificate_pool.md)

# sgcl::crypto::x509::certificate_pool::size

```cpp
size_t size() const noexcept;
```

Returns the number of certificates in the pool.

## Parameters

None.

## Return value

The number of certificates.

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
    auto pool = crypto::x509::certificate_pool::from_file("tests/net/tls_testdata/ca.pem");
    println("{}", pool->size());
}
```

Output:

```text
1
```

## See also

- [empty](empty.md)
- [sgcl::crypto::x509::certificate_pool](../x509-certificate_pool.md)
