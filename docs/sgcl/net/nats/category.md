[sgcl](../../README.md) › [net](../README.md) › [nats](README.md)

# sgcl::net::nats::category

```cpp
const std::error_category& category() noexcept;
```

The error category of [errc](errc.md), named `"nats"`.

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
#include "sgcl/net/nats.h"

using namespace sgcl;

int main() {
    error_code e = net::nats::errc::no_responders;
    println("{}: {}", e.category().name(), e.message());
}
```

Output:

```text
nats: no responders
```

## See also

- [errc](errc.md)
- [make_error_code](make_error_code.md)
