[sgcl](../../../README.md) › [net](../../README.md) › [acme](../README.md) › [manager](README.md)

# sgcl::net::acme::manager::client, async_client

```cpp
expected<acme::client, io::error> client() const;                                // (1)
async::task<expected<acme::client, io::error>> async_client() const noexcept;    // (2)
```

The [client](../client/README.md) of the manager's account, its key read or made and the account registered on first
use, as the manager's own requests have it: for what the manager does not do itself, a revocation, the account's
contact changed.

1. Blocks the calling thread: for a thread of the program, never a worker.
2. Returns a task that does the same, for a task to `co_await`.

## Parameters

None.

## Return value

The client, or the error of the account's making.

## Complexity

One registration the first time, none after it.

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
    net::acme::manager certificates({"example.com", "www.example.com"},
                                    {.directory_url = ca.directory_url(), .accept_terms = true});

    net::tls::identity id = certificates.certificate("example.com");
    net::acme::client acme = certificates.client();
    acme.revoke(id.certificates()[0]);
    println("{}", ca.revoked(id.certificates()[0]));
    certificates.close();
}
```

Output:

```text
true
```

## See also

- [client](../client/README.md)
- [sgcl::net::acme::manager](README.md)
