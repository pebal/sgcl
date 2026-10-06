[sgcl](../../README.md) › [net](../README.md) › [jsonrpc](README.md)

# sgcl::net::jsonrpc::category

```cpp
const std::error_category& category() noexcept;
```

The error category of [errc](errc.md) and of every error object's code, named `"jsonrpc"`.

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
#include "sgcl/encoding.h"
#include "sgcl/io.h"
#include "sgcl/net/jsonrpc.h"

using namespace sgcl;

int main() {
    error_code e = net::jsonrpc::errc::invalid_params;
    println("{} {}: {}", e.category().name(), e.value(), e.message());
}
```

Output:

```text
jsonrpc -32602: invalid params
```

## See also

- [errc](errc.md)
- [make_error_code](make_error_code.md)
