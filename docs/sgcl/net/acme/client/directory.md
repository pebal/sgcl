[sgcl](../../../README.md) › [net](../../README.md) › [acme](../README.md) › [client](README.md)

# sgcl::net::acme::client::directory, async_directory

```cpp
expected<acme::directory, io::error> directory() const;                                // (1)
async::task<expected<acme::directory, io::error>> async_directory() const noexcept;    // (2)
```

The CA's [directory](../directory.md) (RFC 8555 §7.1.1): its resources' URLs, made absolute, and its meta. Read on the
first call that needs it and kept: the later calls answer from memory.

1. Blocks the calling thread: the exchange runs on the scheduler and the thread waits for it. For a thread of the
   program, never a worker.
2. Returns a task that does the same, for a task to `co_await`.

## Parameters

None.

## Return value

The directory, or the error: the transport's, a problem of the CA, `errc::malformed_response` for a directory without
newNonce, newAccount or newOrder.

## Complexity

One request the first time, none after it.

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
    net::acme::client acme(ca.directory_url(), net::acme::account_key());
    net::acme::directory d = acme.directory();
    println("{}", d.new_order.ends_with("/acme/new-order"));
    println("{}", d.renewal_info.empty() ? "no ARI" : "ARI");
}
```

Output:

```text
true
ARI
```

## See also

- [directory](../directory.md)
- [sgcl::net::acme::client](README.md)
