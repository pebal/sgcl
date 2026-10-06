[sgcl](../../../README.md) › [net](../../README.md) › [dkim](../README.md) › [signer](README.md)

# sgcl::net::dkim::signer::selector

```cpp
string selector() const noexcept;
```

The selector, `s=` of the signatures, in lower case: the name under which the domain publishes this key, so that a
domain has several keys at once and changes them one at a time.

## Parameters

None.

## Return value

The selector.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/net/smtp.h"

using namespace sgcl;

int main() {
    auto s = net::dkim::signer::generate("example.com", "S2026-10", net::dkim::algorithm::ed25519_sha256);
    println("{}", s.selector());
}
```

Output:

```text
s2026-10
```

## See also

- [domain](domain.md), [record_name](record_name.md)
- [signer](README.md)
