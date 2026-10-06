[sgcl](../../../README.md) › [net](../../README.md) › [dkim](../README.md) › [signer](README.md)

# sgcl::net::dkim::signer::operator==

```cpp
friend bool operator==(const signer& a, const signer& b) noexcept;
```

Whether the two handles are the same signer: one made by a copy of the other. Two signers of the same key read
twice are two signers.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the signers |

## Return value

`true` for the same signer.

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
    auto s = net::dkim::signer::generate("example.com", "s1", net::dkim::algorithm::ed25519_sha256);
    auto copy = s;
    net::dkim::signer again("example.com", "s1", s.private_key_pem());
    println("{} {}", copy == s, again == s);
}
```

Output:

```text
true false
```

## See also

- [signer](README.md)
