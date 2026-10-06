[sgcl](../../README.md) › [net](../README.md) › [ntp](README.md)

# sgcl::net::ntp::make_error_code

```cpp
error_code make_error_code(errc e) noexcept;
```

An [errc](errc.md) as an `error_code` of the [category](category.md) `"ntp"`; an `errc` converts to one by itself
through it.

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
#include "sgcl/io.h"
#include "sgcl/net/ntp.h"

using namespace sgcl;

int main() {
    error_code e = net::ntp::make_error_code(net::ntp::errc::malformed);
    println("{} {}", e.value(), e.message());
}
```

Output:

```text
3 malformed NTP packet
```

## See also

- [errc](errc.md)
