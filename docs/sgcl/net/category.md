[sgcl](../README.md) › [net](README.md)

# sgcl::net::category

```cpp
#include "sgcl/net/error.h"   // or "sgcl/net.h"

namespace sgcl::net {
    const std::error_category& category() noexcept;
}
```

Returns the error category of the module, the one an [errc](errc.md) belongs to, named `"net"`: the counterpart of
`std::system_category()` for the failures no `errno` names. Its `message` gives the text of each code, and
`"unknown net error"` for a number that is none. One object, made at the first call.

## Parameters

None.

## Return value

The category, the same object at every call.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    const std::error_category& net_errors = net::category();
    println("{}", net_errors.name());
    println("{}", net_errors.message(int(net::errc::host_not_found)));
    println("{}", net_errors.message(99));

    error_code code = net::tcp::listen("no port").error().code();
    println("{}", code.category() == net_errors);
}
```

Output:

```text
net
no such host
unknown net error
true
```

## See also

- [errc](errc.md), [make_error_code](make_error_code.md)
- [lookup_category](lookup_category.md): the resolver's codes
