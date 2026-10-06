[sgcl](../../README.md) › [net](../README.md) › [mqtt](README.md)

# sgcl::net::mqtt::make_error_code

```cpp
error_code make_error_code(errc e) noexcept;
```

An [errc](errc.md) as an `error_code` of the category `"mqtt"`.

## Parameters

| Parameter | Description |
|---|---|
| `e` | the code |

## Return value

The `error_code`.

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
    println("{}", net::mqtt::make_error_code(net::mqtt::errc::disconnected).message());
}
```

Output:

```text
disconnected by the peer
```

## See also

- [errc](errc.md)
- [mqtt](README.md)
