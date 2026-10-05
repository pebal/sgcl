[sgcl](../../../README.md) › [net](../../README.md) › [acme](../README.md) › [client](README.md)

# sgcl::net::acme::client::directory_url

```cpp
string directory_url() const noexcept;
```

The URL of the CA's directory the client was made with.

## Parameters

None.

## Return value

The URL.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/acme.h"

using namespace sgcl;

int main() {
    net::acme::client acme(net::acme::lets_encrypt_staging_url, net::acme::account_key());
    println("{}", acme.directory_url());
}
```

Output:

```text
https://acme-staging-v02.api.letsencrypt.org/directory
```

## See also

- [directory](directory.md)
- [sgcl::net::acme::client](README.md)
