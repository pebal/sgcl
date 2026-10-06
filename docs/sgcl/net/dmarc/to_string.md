[sgcl](../../README.md) › [net](../README.md) › [dmarc](README.md)

# sgcl::net::dmarc::to_string

```cpp
string to_string(policy p) noexcept;    // (1)
string to_string(status s) noexcept;    // (2)
```

1. The policy as a record writes it: `"none"`, `"quarantine"`, `"reject"`.
2. The status as an Authentication-Results field writes it: `"none"`, `"pass"`, `"fail"`, `"temperror"`,
   `"permerror"`.

## Parameters

| Parameter | Description |
|---|---|
| `p` | the policy |
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
    println("{} {}", net::dmarc::to_string(net::dmarc::policy::reject), net::dmarc::to_string(net::dmarc::status::pass));
}
```

Output:

```text
reject pass
```

## See also

- [policy](policy.md), [status](status.md)
- [dmarc](README.md)
