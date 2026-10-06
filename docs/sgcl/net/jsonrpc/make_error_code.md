[sgcl](../../README.md) › [net](../README.md) › [jsonrpc](README.md)

# sgcl::net::jsonrpc::make_error_code

```cpp
error_code make_error_code(errc e) noexcept;
```

An [errc](errc.md) as an `error_code` of the [category](category.md) `"jsonrpc"`; an `errc` converts to one by itself through it.

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
#include "sgcl/encoding.h"
#include "sgcl/io.h"
#include "sgcl/net/jsonrpc.h"

using namespace sgcl;

int main() {
    error_code e = net::jsonrpc::make_error_code(net::jsonrpc::errc::parse_error);
    println("{} {}", e.value(), e.message());
}
```

Output:

```text
-32700 parse error
```

## See also

- [errc](errc.md)
