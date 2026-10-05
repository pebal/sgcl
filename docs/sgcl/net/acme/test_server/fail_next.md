[sgcl](../../../README.md) › [net](../../README.md) › [acme](../README.md) › [test_server](README.md)

# sgcl::net::acme::test_server::fail_next

```cpp
void fail_next(errc code, int count = 1, const string& resource = {},
               duration retry_after = duration::zero()) const;
```

The next `count` signed requests to `resource` answered with the problem of `code`, a Retry-After of `retry_after` with
it when not zero: how a test meets a CA's rate limit, a badNonce, an error of its own. The resources are `new-account`,
`account`, `new-order`, `order`, `authz`, `chall`, `finalize`, `cert`, `revoke-cert` and `key-change`; empty is any.

## Parameters

| Parameter | Description |
|---|---|
| `code` | the problem's type ([errc](../errc.md)) |
| `count` | how many requests; 0 or less asks for none |
| `resource` | the resource; empty: any |
| `retry_after` | the answer's Retry-After; zero: none |

## Return value

None.

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
    acme.register_account({.terms_agreed = true});
    ca.fail_next(net::acme::errc::bad_nonce, 2, "new-order");
    println("{}", acme.new_order({"example.com"}).has_value());   // sent again with fresh nonces
    ca.fail_next(net::acme::errc::server_internal);
    auto refused = acme.new_order({"example.com"});
    println("{}", refused.error().code() == net::acme::errc::server_internal);
}
```

Output:

```text
true
true
```

## See also

- [errc](../errc.md)
- [sgcl::net::acme::test_server](README.md)
