[sgcl](../../README.md) › [net](../README.md) › [ntp](README.md)

# sgcl::net::ntp::category

```cpp
const std::error_category& category() noexcept;
```

The error category of [errc](errc.md), named `"ntp"`.

## Parameters

None.

## Return value

The category, one for the program.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/ntp.h"

using namespace sgcl;

int main() {
    error_code e = net::ntp::errc::unsynchronized;
    println("{}: {}", e.category().name(), e.message());
}
```

Output:

```text
ntp: the server is not synchronized
```

## See also

- [errc](errc.md)
