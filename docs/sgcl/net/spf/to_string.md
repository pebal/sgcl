[sgcl](../../README.md) › [net](../README.md) › [spf](README.md)

# sgcl::net::spf::to_string

```cpp
string to_string(status s) noexcept;
```

The status as Received-SPF and Authentication-Results write it: `"none"`, `"neutral"`, `"pass"`, `"fail"`,
`"softfail"`, `"temperror"`, `"permerror"`.

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
    println("{}", net::spf::to_string(net::spf::status::softfail));
}
```

Output:

```text
softfail
```

## See also

- [status](status.md)
- [spf](README.md)
