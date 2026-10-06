[sgcl](../../README.md) › [net](../README.md) › [amqp](README.md)

# sgcl::net::amqp::make_error_code

```cpp
error_code make_error_code(errc e) noexcept;
```

An [errc](errc.md) as an `error_code` of the [category](category.md) `"amqp"`; an `errc` converts to one by itself through it.

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
#include "sgcl/net/amqp.h"

using namespace sgcl;

int main() {
    error_code e = net::amqp::make_error_code(net::amqp::errc::access_refused);
    println("{} {}", e.value(), e.message());
}
```

Output:

```text
403 access refused
```

## See also

- [errc](errc.md)
