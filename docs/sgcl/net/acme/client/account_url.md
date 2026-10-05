[sgcl](../../../README.md) › [net](../../README.md) › [acme](../README.md) › [client](README.md)

# sgcl::net::acme::client::account_url

```cpp
string account_url() const noexcept;
```

The account's URL, the `kid` its requests are signed with: empty until [register_account](register_account.md) or the
first request that looks it up by the key; the one given by `options::account_url`.

## Parameters

None.

## Return value

The URL, or an empty string.

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
    println("{}", acme.account_url().empty());
    acme.register_account({.terms_agreed = true});
    println("{}", acme.account_url().ends_with("/acme/account/1"));
}
```

Output:

```text
true
true
```

## See also

- [register_account](register_account.md)
- [sgcl::net::acme::client](README.md)
