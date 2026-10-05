[sgcl](../../../README.md) › [net](../../README.md) › [acme](../README.md) › [client](README.md)

# sgcl::net::acme::client::deactivate_account, async_deactivate_account

```cpp
expected<acme::account, io::error> deactivate_account() const;                                // (1)
async::task<expected<acme::account, io::error>> async_deactivate_account() const noexcept;    // (2)
```

The account deactivated (RFC 8555 §7.3.6): for good, nothing it signs is taken again (`errc::unauthorized`); its
certificates stay valid. What a holder does when the key is compromised and a new account is made.

1. Blocks the calling thread: the exchange runs on the scheduler and the thread waits for it. For a thread of the
   program, never a worker.
2. Returns a task that does the same, for a task to `co_await`.

## Parameters

None.

## Return value

The account, its status `deactivated`, or the error.

## Complexity

One request to the CA, and its waits.

## Exceptions

- (1) `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- (2) None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/acme.h"

using namespace sgcl;

int main() {
    net::acme::test_server ca({.skip_validation = true});
    net::acme::client acme(ca.directory_url(), net::acme::account_key());
    acme.register_account({.terms_agreed = true});
    net::acme::account a = acme.deactivate_account();
    println("{}", net::acme::to_string(a.status));
    println("{}", acme.new_order({"example.com"}).error().code() == net::acme::errc::unauthorized);
}
```

Output:

```text
deactivated
true
```

## See also

- [account](../account.md)
- [sgcl::net::acme::client](README.md)
