[sgcl](../../../README.md) › [net](../../README.md) › [acme](../README.md) › [test_server](README.md)

# sgcl::net::acme::test_server::directory_url

```cpp
string directory_url() const noexcept;
```

The URL of the server's directory, what a [client](../client/README.md) and a [manager](../manager/README.md) are given:
`http://127.0.0.1:port/acme/directory`, `https://` with `options::tls`.

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
    net::acme::test_server ca;
    net::acme::client acme(ca.directory_url(), net::acme::account_key());
    println("{}", acme.directory()->new_nonce.ends_with("/acme/new-nonce"));
}
```

Output:

```text
true
```

## See also

- [roots](roots.md)
- [sgcl::net::acme::test_server](README.md)
