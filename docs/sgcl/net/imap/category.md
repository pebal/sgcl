[sgcl](../../README.md) › [net](../README.md) › [imap](README.md)

# sgcl::net::imap::category

```cpp
#include "sgcl/net/imap/error.h"   // or "sgcl/net/imap.h"

namespace sgcl::net::imap {
    const std::error_category& category() noexcept;
}
```

Returns the error category of [errc](errc.md), named `"imap"`: the one object every `error_code` of the module's codes
refers to, its `message()` the sentence of each code.

## Parameters

None.

## Return value

The category, one object for the program's life.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/imap.h"

using namespace sgcl;

int main() {
    const std::error_category& c = net::imap::category();
    println("{}", c.name());
    println("{}", c.message(int(net::imap::errc::over_quota)));
}
```

Output:

```text
imap
over quota
```

## See also

- [errc](errc.md), [make_error_code](make_error_code.md)
- [sgcl::net::imap](README.md)
