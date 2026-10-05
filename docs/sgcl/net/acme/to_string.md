[sgcl](../../README.md) › [net](../README.md) › [acme](README.md)

# sgcl::net::acme::to_string

```cpp
const char* to_string(status s) noexcept;
```

The name of a [status](status.md) as RFC 8555 writes it, `"pending"`, `"valid"`; `"unknown"` for a value of no
enumerator.

## Parameters

| Parameter | Description |
|---|---|
| `s` | the status |

## Return value

The name, a literal.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/acme.h"

using namespace sgcl;

int main() {
    println("{}", net::acme::to_string(net::acme::status::processing));
}
```

Output:

```text
processing
```

## See also

- [status](status.md)
- [net::acme](README.md)
