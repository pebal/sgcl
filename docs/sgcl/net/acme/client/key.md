[sgcl](../../../README.md) › [net](../../README.md) › [acme](../README.md) › [client](README.md)

# sgcl::net::acme::client::key

```cpp
account_key key() const noexcept;
```

The key the client signs with: the one it was made with, or the new one after [change_key](change_key.md).

## Parameters

None.

## Return value

The key, a copy of the handle.

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
    net::acme::test_server ca({.skip_validation = true});
    net::acme::client acme(ca.directory_url(), net::acme::account_key());
    acme.register_account({.terms_agreed = true});
    net::acme::account_key next(net::acme::key_algorithm::eddsa);
    acme.change_key(next);
    println("{}", acme.key() == next);
}
```

Output:

```text
true
```

## See also

- [change_key](change_key.md)
- [sgcl::net::acme::client](README.md)
