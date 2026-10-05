[sgcl](../../../README.md) › [net](../../README.md) › [acme](../README.md) › [client](README.md)

# sgcl::net::acme::client::change_key, async_change_key

```cpp
expected<void, io::error> change_key(const account_key& next) const;                         // (1)
async::task<expected<void, io::error>> async_change_key(account_key next) const noexcept;    // (2)
```

The account's key rolled over to `next` (RFC 8555 §7.3.5): a request of the old key carrying one of the new (its JWK,
the account, the old key's JWK), which proves the holder has both. The client signs with `next` from then on; the old
key has no account any more.

1. Blocks the calling thread: the exchange runs on the scheduler and the thread waits for it. For a thread of the
   program, never a worker.
2. Returns a task that does the same, for a task to `co_await`.

## Parameters

| Parameter | Description |
|---|---|
| `next` | the new key, which no account of the CA may have |

## Return value

Nothing, or the error: `errc::unsupported` for a CA without keyChange, a 409 for a key that is another account's (the
client's key unchanged).

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
    net::acme::account_key old_key = acme.key();
    net::acme::account_key next(net::acme::key_algorithm::es384);
    acme.change_key(next);
    println("{}", acme.key() == next);
    net::acme::client by_old(ca.directory_url(), old_key);
    println("{}", by_old.account().error().code() == net::acme::errc::account_does_not_exist);
}
```

Output:

```text
true
true
```

## See also

- [key](key.md)
- [account_key](../account_key/README.md)
- [sgcl::net::acme::client](README.md)
