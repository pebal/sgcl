[sgcl](../../../README.md) › [net](../../README.md) › [acme](../README.md) › [client](README.md)

# sgcl::net::acme::client::update_account, async_update_account

```cpp
expected<acme::account, io::error> update_account(const vector<string>& contact) const;    // (1)
async::task<expected<acme::account, io::error>>                                            // (2)
    async_update_account(vector<string> contact) const noexcept;
```

The account's contact replaced (RFC 8555 §7.3.2): the URLs the CA reaches its holder at.

1. Blocks the calling thread: the exchange runs on the scheduler and the thread waits for it. For a thread of the
   program, never a worker.
2. Returns a task that does the same, for a task to `co_await`.

## Parameters

| Parameter | Description |
|---|---|
| `contact` | the new contact URLs, `mailto:` ones; empty removes them |

## Return value

The account as updated, or the error: `errc::unsupported_contact` and `invalid_contact`.

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
    net::acme::account a = acme.update_account({"mailto:ops@example.com"});
    println("{}", a.contact[0]);
}
```

Output:

```text
mailto:ops@example.com
```

## See also

- [account](../account.md)
- [sgcl::net::acme::client](README.md)
