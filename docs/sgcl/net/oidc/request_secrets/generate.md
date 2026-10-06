[sgcl](../../../README.md) › [net](../../README.md) › [oidc](../README.md) › [request_secrets](README.md)

# sgcl::net::oidc::request_secrets::generate

```cpp
static request_secrets generate();
```

A new state and nonce, 128 bits each of the system's random generator ([crypto::random](../../../crypto/random/README.md)),
base64url without padding. One per authorization request.

## Parameters

None.

## Return value

The state and the nonce.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/oidc.h"

using namespace sgcl;

int main() {
    net::oidc::request_secrets a = net::oidc::request_secrets::generate();
    net::oidc::request_secrets b = net::oidc::request_secrets::generate();
    println("{}", a.state == b.state);
}
```

Output:

```text
false
```

## See also

- [request_secrets](README.md)
