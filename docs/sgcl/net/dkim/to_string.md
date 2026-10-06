[sgcl](../../README.md) › [net](../README.md) › [dkim](README.md)

# sgcl::net::dkim::to_string

```cpp
string to_string(status s) noexcept;
```

The status as an Authentication-Results field writes it (RFC 8601 §2.7.1): `"none"`, `"pass"`, `"fail"`,
`"policy"`, `"neutral"`, `"temperror"`, `"permerror"`.

## Parameters

| Parameter | Description |
|---|---|
| `s` | the status |

## Return value

Its name.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/net/smtp.h"

using namespace sgcl;

int main() {
    println("{}", net::dkim::to_string(net::dkim::status::temperror));
}
```

Output:

```text
temperror
```

## See also

- [status](status.md)
- [dkim](README.md)
