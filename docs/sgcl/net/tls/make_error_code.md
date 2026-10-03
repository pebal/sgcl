[sgcl](../../README.md) › [net](../README.md) › [tls](README.md)

# sgcl::net::tls::make_error_code

```cpp
#include "sgcl/net/tls/error.h"   // or "sgcl/net/tls.h"

namespace sgcl::net::tls {
    error_code make_error_code(alert a) noexcept;
}
```

Returns the `error_code` of an alert this side sent: `a` in the [category](category.md) of TLS. It is the function
`std::error_code` finds by argument-dependent lookup when an [alert](alert.md) converts to it, so an alert converts
to an `error_code` and compares with one by itself, as `std::make_error_code` serves `std::errc`. An alert the peer
sent has another code, which this one does not equal: [alert_of](alert_of.md) gives the alert of either.

## Parameters

| Parameter | Description |
|---|---|
| `a` | the alert |

## Return value

The `error_code` of value `a`'s number in the category of TLS.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/tls.h"

using namespace sgcl;

int main() {
    error_code code = net::tls::make_error_code(net::tls::alert::bad_certificate);
    println("{} {} {}", code.value(), code.category().name(), code.message());
    println("{}", code == net::tls::alert::bad_certificate);
}
```

Output:

```text
42 tls tls: bad certificate
true
```

## See also

- [alert](alert.md), [category](category.md)
- [io::error](../../io/error/README.md)
- [net::tls](README.md)
