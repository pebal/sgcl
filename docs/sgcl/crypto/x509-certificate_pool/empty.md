[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [certificate_pool](../x509-certificate_pool.md)

# sgcl::crypto::x509::certificate_pool::empty

```cpp
bool empty() const noexcept;
```

Checks whether the pool has no certificate.

## Parameters

None.

## Return value

`true` when the pool has none, `false` otherwise.

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
    auto pool = crypto::x509::certificate_pool::from_pem("no certificates here");
    println("{}", pool.empty());
}
```

Output:

```text
true
```

## See also

- [size](size.md)
- [sgcl::crypto::x509::certificate_pool](../x509-certificate_pool.md)
