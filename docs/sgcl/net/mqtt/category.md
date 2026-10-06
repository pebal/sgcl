[sgcl](../../README.md) › [net](../README.md) › [mqtt](README.md)

# sgcl::net::mqtt::category

```cpp
const std::error_category& category() noexcept;
```

The error category of [errc](errc.md), named `"mqtt"`.

## Parameters

None.

## Return value

The category.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/net/mqtt.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    println("{}", net::mqtt::category().name());
}
```

Output:

```text
mqtt
```

## See also

- [errc](errc.md)
- [mqtt](README.md)
