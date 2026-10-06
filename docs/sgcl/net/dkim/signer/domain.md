[sgcl](../../../README.md) › [net](../../README.md) › [dkim](../README.md) › [signer](README.md)

# sgcl::net::dkim::signer::domain

```cpp
string domain() const noexcept;
```

The signing domain, `d=` of the signatures, in lower case.

## Parameters

None.

## Return value

The domain.

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
    auto s = net::dkim::signer::generate("Mail.Example.COM", "s1", net::dkim::algorithm::ed25519_sha256);
    println("{}", s.domain());
}
```

Output:

```text
mail.example.com
```

## See also

- [selector](selector.md)
- [signer](README.md)
