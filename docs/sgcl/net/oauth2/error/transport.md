[sgcl](../../../README.md) › [net](../../README.md) › [oauth2](../README.md) › [error](README.md)

# sgcl::net::oauth2::error::transport

```cpp
optional<io::error> transport() const noexcept;
```

The failure of the exchange, when the error is one: the connection refused or lost, a timeout of the config's `http`
client, a stop (`operation_canceled`), a response that is not one of OAuth (`net::errc::malformed_response`), an empty
endpoint (`invalid_argument`). nullopt for an error of the server.

## Parameters

None.

## Return value

The [io::error](../../../io/error/README.md), or nullopt.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/oauth2.h"

using namespace sgcl;

int main() {
    net::oauth2::config cfg;
    cfg.endpoints.token = "http://127.0.0.1:1/token";  // nothing listens there
    auto t = cfg.client_credentials();
    println("{}", t.error().transport()->code() == std::errc::connection_refused);
}
```

Output:

```text
true
```

## See also

- [error](README.md)
