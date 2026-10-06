[sgcl](../../../README.md) › [net](../../README.md) › [oauth2](../README.md) › [error](README.md)

# sgcl::net::oauth2::error::error

```cpp
error() noexcept;                                                               // (1)
error(const string& code, const string& description, const string& uri = {},    // (2)
      int status = 0) noexcept;
explicit error(const io::error& transport) noexcept;                            // (3)
```

1. No error: every member empty.
2. An error of the protocol, as a server would answer it.
3. A failure of the exchange.

## Parameters

| Parameter | Description |
|---|---|
| `code` | the code, `invalid_grant` |
| `description` | what it says to a person |
| `uri` | a page about it |
| `status` | the HTTP status |
| `transport` | the failure of the exchange |

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
    net::oauth2::error grant("invalid_grant", "the code expired", "", 400);
    io::error timeout(std::make_error_code(std::errc::timed_out), "token", "as.example");
    net::oauth2::error lost(timeout);
    println("{}", grant.message());
    println("{}", lost.transport().has_value());
}
```

Output:

```text
invalid_grant: the code expired
true
```

## See also

- [error](README.md)
