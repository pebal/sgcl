[sgcl](../../README.md) › [net](../README.md) › [amqp](README.md)

# sgcl::net::amqp::category

```cpp
const std::error_category& category() noexcept;
```

The error category of [errc](errc.md), named `"amqp"`.

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
#include "sgcl/net/amqp.h"

using namespace sgcl;

int main() {
    error_code e = net::amqp::errc::not_found;
    println("{} {}: {}", e.category().name(), e.value(), e.message());
}
```

Output:

```text
amqp 404: not found
```

## See also

- [errc](errc.md)
- [make_error_code](make_error_code.md)
