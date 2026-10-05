[sgcl](../../../README.md) › [net](../../README.md) › [acme](../README.md) › [client](README.md)

# sgcl::net::acme::client::account, async_account

```cpp
expected<acme::account, io::error> account() const;                                // (1)
async::task<expected<acme::account, io::error>> async_account() const noexcept;    // (2)
```

The account as the CA has it now: a POST-as-GET of its URL (RFC 8555 §7.3.3), the URL looked up by the key first when
the client does not know it.

1. Blocks the calling thread: the exchange runs on the scheduler and the thread waits for it. For a thread of the
   program, never a worker.
2. Returns a task that does the same, for a task to `co_await`.

## Parameters

None.

## Return value

The account, or the error: `errc::account_does_not_exist` for a key without one.

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
    net::acme::test_server ca;
    net::acme::account_key key;
    net::acme::client first(ca.directory_url(), key);
    first.register_account({.contact = {"mailto:admin@example.com"}, .terms_agreed = true});
    net::acme::client later(ca.directory_url(), key);   // of the same key, without the URL
    net::acme::account a = later.account();
    println("{} {}", a.contact[0], a.url == first.account_url());
}
```

Output:

```text
mailto:admin@example.com true
```

## See also

- [account](../account.md)
- [sgcl::net::acme::client](README.md)
