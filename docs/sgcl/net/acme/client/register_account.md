[sgcl](../../../README.md) › [net](../../README.md) › [acme](../README.md) › [client](README.md)

# sgcl::net::acme::client::register_account, async_register_account

```cpp
expected<acme::account, io::error> register_account(const account_options& o = {}) const;    // (1)
async::task<expected<acme::account, io::error>>                                              // (2)
    async_register_account(account_options o = {}) const noexcept;
```

A new account of the key (RFC 8555 §7.3), or the account the key has, which the CA answers with: its URL becomes the
client's `kid`. With `o.external_account`, the binding signs the key under the CA's MAC key (§7.3.4); a CA whose
directory says it requires one refuses an account without it, which the client does itself, before a request
(`errc::external_account_required`). With `o.only_return_existing`, no account is made:
`errc::account_does_not_exist` when the key has none.

1. Blocks the calling thread: the exchange runs on the scheduler and the thread waits for it. For a thread of the
   program, never a worker.
2. Returns a task that does the same, for a task to `co_await`.

## Parameters

| Parameter | Description |
|---|---|
| `o` | the contact, the terms agreed to, an external account binding ([account_options](../account_options.md)) |

## Return value

The account, or the error: `errc::malformed` for terms not agreed to, `errc::unsupported_contact` and
`invalid_contact`, `errc::external_account_required`, `errc::unauthorized` for a binding the CA does not take.

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
    net::acme::test_server ca({.terms_of_service = "https://ca.example/terms"});
    net::acme::client acme(ca.directory_url(), net::acme::account_key());
    println("{}", acme.register_account().error().message());
    net::acme::account a =
        acme.register_account({.contact = {"mailto:admin@example.com"}, .terms_agreed = true});
    println("{} {}", net::acme::to_string(a.status), a.contact[0]);
}
```

Output:

```text
acme new-account the terms of service must be agreed to: acme: the request is malformed
valid mailto:admin@example.com
```

## See also

- [account_options](../account_options.md), [account](../account.md)
- [sgcl::net::acme::client](README.md)
