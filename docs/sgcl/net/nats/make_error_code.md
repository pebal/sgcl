[sgcl](../../README.md) › [net](../README.md) › [nats](README.md)

# sgcl::net::nats::make_error_code

```cpp
error_code make_error_code(errc e) noexcept;
```

An [errc](errc.md) as an `error_code` of the [category](category.md) `"nats"`; an `errc` converts to one by itself through it.

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
#include "sgcl/net/nats.h"

using namespace sgcl;

int main() {
    error_code e = net::nats::make_error_code(net::nats::errc::max_payload);
    println("{} {}", e.value(), e.message());
}
```

Output:

```text
5 maximum payload exceeded
```

## See also

- [errc](errc.md)
